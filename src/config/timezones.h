/*
 * CYD Game Clock - Time zones
 *
 * Each zone is a POSIX TZ rule, which holds both the offset from UTC and the
 * daylight-saving dates, so summer time switches on and off by itself.
 * The key is what is stored in flash, so entries can be added or re-ordered
 * without changing anyone's setting. Entry 0 is the default.
 */

#ifndef TIMEZONES_H
#define TIMEZONES_H

struct TimeZoneInfo {
  const char* key;    // stored in NVS
  const char* label;  // shown on the settings page
  const char* posix;  // TZ rule
};

static const TimeZoneInfo TIMEZONES[] = {
    {"london", "London, Edinburgh (GMT / BST)", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"utc", "UTC (no daylight saving)", "UTC0"},
    {"dublin", "Dublin (GMT / IST)", "IST-1GMT0,M10.5.0,M3.5.0/1"},
    {"lisbon", "Lisbon, Canary Islands (WET / WEST)", "WET0WEST,M3.5.0/1,M10.5.0"},
    {"reykjavik", "Reykjavik (GMT, no daylight saving)", "GMT0"},
    {"cet", "Paris, Berlin, Madrid, Rome, Amsterdam (CET / CEST)", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"lagos", "Lagos, West Africa (WAT)", "WAT-1"},
    {"eet", "Athens, Helsinki, Kyiv (EET / EEST)", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"cairo", "Cairo (EET / EEST)", "EET-2EEST,M4.5.5/0,M10.5.4/24"},
    {"johannesburg", "Johannesburg (SAST)", "SAST-2"},
    {"istanbul", "Istanbul (UTC+3)", "<+03>-3"},
    {"moscow", "Moscow (MSK)", "MSK-3"},
    {"nairobi", "Nairobi, East Africa (EAT)", "EAT-3"},
    {"tehran", "Tehran (UTC+3:30)", "<+0330>-3:30"},
    {"dubai", "Dubai, Abu Dhabi (UTC+4)", "<+04>-4"},
    {"karachi", "Karachi (PKT)", "PKT-5"},
    {"india", "India (IST)", "IST-5:30"},
    {"kathmandu", "Kathmandu (UTC+5:45)", "<+0545>-5:45"},
    {"dhaka", "Dhaka (UTC+6)", "<+06>-6"},
    {"bangkok", "Bangkok, Jakarta, Hanoi (UTC+7)", "<+07>-7"},
    {"china", "Beijing, Hong Kong, Taipei (CST)", "CST-8"},
    {"singapore", "Singapore, Kuala Lumpur (UTC+8)", "<+08>-8"},
    {"perth", "Perth (AWST)", "AWST-8"},
    {"tokyo", "Tokyo (JST)", "JST-9"},
    {"seoul", "Seoul (KST)", "KST-9"},
    {"darwin", "Darwin (ACST)", "ACST-9:30"},
    {"adelaide", "Adelaide (ACST / ACDT)", "ACST-9:30ACDT,M10.1.0,M4.1.0/3"},
    {"brisbane", "Brisbane (AEST)", "AEST-10"},
    {"sydney", "Sydney, Melbourne, Canberra, Hobart (AEST / AEDT)", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"auckland", "Auckland, Wellington (NZST / NZDT)", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
    {"honolulu", "Hawaii (HST)", "HST10"},
    {"anchorage", "Alaska (AKST / AKDT)", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"pacific", "US & Canada Pacific (PST / PDT)", "PST8PDT,M3.2.0,M11.1.0"},
    {"arizona", "Arizona (MST, no daylight saving)", "MST7"},
    {"mountain", "US & Canada Mountain (MST / MDT)", "MST7MDT,M3.2.0,M11.1.0"},
    {"central", "US & Canada Central (CST / CDT)", "CST6CDT,M3.2.0,M11.1.0"},
    {"mexico", "Mexico City (CST)", "CST6"},
    {"eastern", "US & Canada Eastern (EST / EDT)", "EST5EDT,M3.2.0,M11.1.0"},
    {"bogota", "Bogota, Lima, Quito (UTC-5)", "<-05>5"},
    {"atlantic", "Atlantic Canada (AST / ADT)", "AST4ADT,M3.2.0,M11.1.0"},
    {"santiago", "Santiago (UTC-4 / -3)", "<-04>4<-03>,M9.1.6/24,M4.1.6/24"},
    {"newfoundland", "Newfoundland (NST / NDT)", "NST3:30NDT,M3.2.0,M11.1.0"},
    {"saopaulo", "Sao Paulo, Buenos Aires (UTC-3)", "<-03>3"},
};

static const int TIMEZONE_COUNT = sizeof(TIMEZONES) / sizeof(TIMEZONES[0]);

// Index of the zone with this key, or 0 (the default) if unknown
inline int timezoneIndexForKey(const char* key) {
  for (int i = 0; i < TIMEZONE_COUNT; i++) {
    if (strcmp(TIMEZONES[i].key, key) == 0) return i;
  }
  return 0;
}

#endif // TIMEZONES_H
