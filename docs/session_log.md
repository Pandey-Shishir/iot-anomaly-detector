# Data Collection Session Log

## File Naming
Each session file is named automatically from the condition and timestamp:
`{time_of_day}_{location}_{light_condition}_{YYYYMMDD}_{HHMM}.csv`

The filename itself fully describes the session. Only additional context
not captured in the filename is recorded here.

---

## Sessions

| File | Rows | Notes |
|---|---|---|
| night_indoor_artificial_20260326_2354.csv | 1072 | Very stable -- LDR barely varies (242-248). Clean baseline. |
| morning_indoor_cloudy_20260327_0845.csv | 1073 | LDR varies widely (51-678) as morning light changed gradually. |
| day_indoor_cloudy_20260327_1321.csv | 1072 | Good mid-range light, stable temperature. |
| day_indoor_sunny_20260327_1448.csv | 1027 | Near window in direct sun, LDR high (617-948). |
| day_indoor_sunny_20260327_1457.csv | 1027 | Second sunny session, wider LDR range (407-958). |
| day_indoor_artificial_20260327_1509.csv | 1026 | Dim artificial only, LDR low (22-353). |
| day_outdoor_sunny_20260327_1520.csv | 1026 | Bright direct sun, LDR nearly maxed (953-984). |
| day_outdoor_cloudy_20260327_1526.csv | 1026 | Outdoor overcast, temp higher than indoor (38-40C). |

**Total rows: 8349 across 8 sessions**

---
