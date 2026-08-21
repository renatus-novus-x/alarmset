#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <x68k/iocs.h>

#define ALARM_QUERY 2
#define ALARM_WEEKDAY_ANY 0x0fU
#define ALARM_FIELD_ANY 0xffU
#define NO_AUTO_POWER_OFF 0
#define NO_DISPLAY_CONTROL (-1)

static const char *const weekday_names[] = {
  "sun", "mon", "tue", "wed", "thu", "fri", "sat"
};

static void print_usage(FILE *stream){
  fputs("Usage:\n", stream);
  fputs("  alarmset                              Show status and usage\n", stream);
  fputs("  alarmset status                       Show the saved alarm\n", stream);
  fputs("  alarmset HH:MM [--off-after MINUTES]  Set a daily alarm\n", stream);
  fputs("  alarmset DAY HH:MM [--off-after MINUTES]\n", stream);
  fputs("                                        Set a weekly alarm (sun..sat)\n", stream);
  fputs("  alarmset DATE HH:MM [--off-after MINUTES]\n", stream);
  fputs("                                        Set a monthly alarm (1..31)\n", stream);
  fputs("  alarmset off                          Disable the alarm\n", stream);
  fputs("  alarmset -? | --help                  Show this help\n", stream);
}

static int ascii_equal(const char *left, const char *right){
  while (*left != '\0' && *right != '\0') {
    if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
      return 0;
    }
    ++left;
    ++right;
  }
  return *left == '\0' && *right == '\0';
}

static int parse_number(const char *text, int minimum, int maximum, int *value){
  int number = 0;
  int digits = 0;
  int digit;

  while (*text != '\0') {
    if (!isdigit((unsigned char)*text)) {
      return 0;
    }
    digit = *text - '0';
    if (number > (maximum - digit) / 10) {
      return 0;
    }
    number = number * 10 + digit;
    ++digits;
    ++text;
  }

  if (digits == 0 || number < minimum || number > maximum) {
    return 0;
  }

  *value = number;
  return 1;
}

static int parse_time(const char *text, int *hour, int *minute){
  if (strlen(text) != 5 || text[2] != ':' ||
      !isdigit((unsigned char)text[0]) ||
      !isdigit((unsigned char)text[1]) ||
      !isdigit((unsigned char)text[3]) ||
      !isdigit((unsigned char)text[4])) {
    return 0;
  }

  *hour = (text[0] - '0') * 10 + text[1] - '0';
  *minute = (text[3] - '0') * 10 + text[4] - '0';
  return *hour <= 23 && *minute <= 59;
}

static int parse_weekday(const char *text, int *weekday){
  int i;

  for (i = 0; i < 7; ++i) {
    if (ascii_equal(text, weekday_names[i])) {
      *weekday = i;
      return 1;
    }
  }
  return 0;
}

static unsigned int to_bcd(int value){
  return (unsigned int)(((value / 10) << 4) | (value % 10));
}

static int bcd_is_valid(unsigned int value, int maximum){
  int binary;

  if ((value & 0x0fU) > 9U || ((value >> 4) & 0x0fU) > 9U) {
    return 0;
  }
  binary = (int)(((value >> 4) & 0x0fU) * 10U + (value & 0x0fU));
  return binary <= maximum;
}

static void print_bcd_field(unsigned int value, int maximum){
  if (value == ALARM_FIELD_ANY) {
    fputs("any", stdout);
  } else if (bcd_is_valid(value, maximum)) {
    printf("%02u", ((value >> 4) & 0x0fU) * 10U + (value & 0x0fU));
  } else {
    printf("$%02X", value);
  }
}

static void print_status(void){
  int alarm;
  int power_off_minutes;
  int action;
  int enabled;
  uint32_t value;
  unsigned int weekday;
  unsigned int date;
  unsigned int hour;
  unsigned int minute;

  enabled = _iocs_alarmmod(ALARM_QUERY);
  (void)_iocs_alarmget(&alarm, &power_off_minutes, &action);

  value = (uint32_t)alarm;
  weekday = (unsigned int)((value >> 24) & 0x0fU);
  date = (unsigned int)((value >> 16) & 0xffU);
  hour = (unsigned int)((value >> 8) & 0xffU);
  minute = (unsigned int)(value & 0xffU);

  puts("RTC wake-up alarm:");
  printf("  Enabled: %s\n", enabled != 0 ? "yes" : "no");

  fputs("  Weekday: ", stdout);
  if (weekday == ALARM_WEEKDAY_ANY) {
    puts("any");
  } else if (weekday < 7U) {
    puts(weekday_names[weekday]);
  } else {
    printf("$%X\n", weekday);
  }

  fputs("  Date: ", stdout);
  print_bcd_field(date, 31);
  putchar('\n');

  fputs("  Time: ", stdout);
  print_bcd_field(hour, 23);
  putchar(':');
  print_bcd_field(minute, 59);
  putchar('\n');

  if (power_off_minutes <= 0) {
    puts("  Automatic power-off: disabled");
  } else {
    printf("  Automatic power-off: %d minute(s)\n", power_off_minutes);
  }

  if (action == NO_DISPLAY_CONTROL) {
    puts("  Wake action: normal boot, display unchanged");
  } else if (action == 0) {
    puts("  Wake action: normal boot, computer display mode");
  } else {
    printf("  Wake action: $%08lX\n", (unsigned long)(uint32_t)action);
  }
}

static uint32_t make_alarm(int weekday, int date, int hour, int minute){
  return ((uint32_t)weekday << 24) |
         ((uint32_t)date << 16) |
         ((uint32_t)to_bcd(hour) << 8) |
         (uint32_t)to_bcd(minute);
}

static int parse_power_off_option(int argc, char *argv[], int first,
                                  int *minutes){
  *minutes = NO_AUTO_POWER_OFF;

  if (argc == first) {
    return 1;
  }
  if (argc != first + 2 || strcmp(argv[first], "--off-after") != 0) {
    return 0;
  }
  return parse_number(argv[first + 1], 1, INT_MAX, minutes);
}

static int configure_alarm(int weekday, int date, int hour, int minute,
                           int power_off_minutes){
  uint32_t requested;
  int stored;
  int stored_power_off_minutes;
  int action;

  requested = make_alarm(weekday, date, hour, minute);
  (void)_iocs_alarmset((int)requested, power_off_minutes,
                       NO_DISPLAY_CONTROL);
  (void)_iocs_alarmget(&stored, &stored_power_off_minutes, &action);

  if (_iocs_alarmmod(ALARM_QUERY) == 0 ||
      ((((uint32_t)stored ^ requested) & 0x0fffffffUL) != 0U) ||
      (power_off_minutes > 0 &&
       stored_power_off_minutes != power_off_minutes)) {
    fputs("Failed to store the RTC alarm.\n", stderr);
    return 1;
  }

  puts("RTC wake-up alarm configured.");
  print_status();
  return 0;
}

static int disable_alarm(void){
  (void)_iocs_alarmmod(0);
  if (_iocs_alarmmod(ALARM_QUERY) != 0) {
    fputs("Failed to disable the RTC alarm.\n", stderr);
    return 1;
  }

  puts("RTC wake-up alarm disabled.");
  return 0;
}

int main(int argc, char *argv[]){
  int hour;
  int minute;
  int weekday;
  int date;
  int power_off_minutes;

  if (argc == 1) {
    print_status();
    putchar('\n');
    print_usage(stdout);
    return 0;
  }

  if (argc == 2) {
    if (ascii_equal(argv[1], "status")) {
      print_status();
      return 0;
    }
    if (ascii_equal(argv[1], "off")) {
      return disable_alarm();
    }
    if (strcmp(argv[1], "-?") == 0 || strcmp(argv[1], "--help") == 0) {
      print_usage(stdout);
      return 0;
    }
  }

  if (argc >= 2 && parse_time(argv[1], &hour, &minute) &&
      parse_power_off_option(argc, argv, 2, &power_off_minutes)) {
    return configure_alarm(ALARM_WEEKDAY_ANY, ALARM_FIELD_ANY,
                           hour, minute, power_off_minutes);
  }

  if (argc >= 3 && parse_time(argv[2], &hour, &minute) &&
      parse_power_off_option(argc, argv, 3, &power_off_minutes)) {
    if (parse_weekday(argv[1], &weekday)) {
      return configure_alarm(weekday, ALARM_FIELD_ANY, hour, minute,
                             power_off_minutes);
    }
    if (parse_number(argv[1], 1, 31, &date)) {
      return configure_alarm(ALARM_WEEKDAY_ANY, (int)to_bcd(date),
                             hour, minute, power_off_minutes);
    }
  }

  fputs("Invalid arguments.\n\n", stderr);
  print_usage(stderr);
  return 1;
}
