use std::io;
#[cfg(target_os = "freebsd")]
use std::process::Command;

pub struct SnapshotInfo {
    pub build_number: String,
    pub release_date: String,
    pub moscow_time: String,
}

impl SnapshotInfo {
    pub fn current() -> io::Result<Self> {
        let build_number = Self::get_build_number()?;
        let release_date = Self::get_release_date()?;
        let moscow_time = Self::get_moscow_time()?;

        Ok(Self {
            build_number,
            release_date,
            moscow_time,
        })
    }

    fn get_build_number() -> io::Result<String> {
        #[cfg(target_os = "freebsd")]
        {
            let output = Command::new("/usr/bin/uname").arg("-K").output()?;

            if output.status.success() {
                let kernel_version = String::from_utf8_lossy(&output.stdout);
                Ok(kernel_version.trim().to_owned())
            } else {
                Ok("unknown".to_owned())
            }
        }

        #[cfg(not(target_os = "freebsd"))]
        {
            Ok("16.0-CURRENT-20261113".to_owned())
        }
    }

    fn get_release_date() -> io::Result<String> {
        #[cfg(target_os = "freebsd")]
        {
            let output = Command::new("/bin/date")
                .arg("-r")
                .arg("/boot/kernel/kernel")
                .arg("+%Y-%m-%d %H:%M:%S")
                .output()?;

            if output.status.success() {
                let date = String::from_utf8_lossy(&output.stdout);
                Ok(date.trim().to_owned())
            } else {
                Ok("unknown".to_owned())
            }
        }

        #[cfg(not(target_os = "freebsd"))]
        {
            Ok("2026-11-13 15:42:08".to_owned())
        }
    }

    fn get_moscow_time() -> io::Result<String> {
        #[cfg(target_os = "freebsd")]
        {
            let output = Command::new("/bin/date")
                .env("TZ", "Europe/Moscow")
                .arg("+%Y-%m-%d %H:%M:%S %Z")
                .output()?;

            if output.status.success() {
                let time = String::from_utf8_lossy(&output.stdout);
                Ok(time.trim().to_owned())
            } else {
                Ok("unknown".to_owned())
            }
        }

        #[cfg(not(target_os = "freebsd"))]
        {
            use std::time::{SystemTime, UNIX_EPOCH};
            let now = SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap_or_default()
                .as_secs();

            // Moscow is UTC+3
            let moscow_offset: u64 = 3 * 3600;
            let moscow_time = now + moscow_offset;

            let days_since_epoch = moscow_time / 86400;

            let mut year: u64 = 1970;
            let mut days = days_since_epoch;

            loop {
                let days_in_year = if Self::is_leap_year(year) { 366 } else { 365 };
                if days < days_in_year {
                    break;
                }
                days -= days_in_year;
                year += 1;
            }

            let months: [u64; 12] = if Self::is_leap_year(year) {
                [31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
            } else {
                [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
            };

            let mut month = 0usize;
            let mut day = days + 1;
            for (i, &days_in_month) in months.iter().enumerate() {
                if day <= days_in_month {
                    month = i + 1;
                    break;
                }
                day -= days_in_month;
            }

            let seconds_today = moscow_time % 86400;
            let hour = seconds_today / 3600;
            let minute = (seconds_today % 3600) / 60;
            let second = seconds_today % 60;

            Ok(format!(
                "{:04}-{:02}-{:02} {:02}:{:02}:{:02} MSK",
                year, month, day, hour, minute, second
            ))
        }
    }

    #[cfg(not(target_os = "freebsd"))]
    fn is_leap_year(year: u64) -> bool {
        (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)
    }

    pub fn display(&self) -> String {
        format!(
            "AmeOS Snapshot Information\n\
             ===========================\n\
             \n\
             Build number:    {}\n\
             Release date:    {}\n\
             Moscow time:     {}\n",
            self.build_number, self.release_date, self.moscow_time
        )
    }
}
