# Data

## CSV Format
Each session is saved as a separate CSV file with three columns:

| Column | Type | Description |
|---|---|---|
| timestamp_ms | integer | Arduino uptime in milliseconds |
| LightMean | float | Average of 8 burst ADC reads from LDR (0-1023 range) |
| TempMean | float | Average of 8 burst ADC reads converted to Celsius |

## File Naming Convention
Files are named by session condition and timestamp:
```
{time_of_day}_{location}_{light_condition}_{YYYYMMDD}_{HHMM}.csv
```

Examples:
- `morning_indoor_sunny_20260326_0830.csv`
- `night_indoor_artificial_20260326_2210.csv`
- `day_outdoor_cloudy_20260327_1430.csv`

## Raw Data Files
Raw CSV files are not stored in this repository as they are large binary-equivalent
files not suitable for version control. Session metadata is recorded in
`docs/session_log.md`.

## Sample Rate
200ms per row -- approximately 1200 rows per 4 minute session.
