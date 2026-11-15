use std::io;
#[cfg(target_os = "freebsd")]
use std::process::Command;

#[derive(Debug, Clone)]
pub struct BootEnvironment {
    pub name: String,
    pub active: bool,
    #[allow(dead_code)]
    pub mountpoint: String,
    #[allow(dead_code)]
    pub creation: String,
}

pub fn list_boot_environments() -> io::Result<Vec<BootEnvironment>> {
    #[cfg(target_os = "freebsd")]
    {
        let output = Command::new("/usr/sbin/bectl")
            .arg("list")
            .arg("-H")
            .output()?;

        if !output.status.success() {
            return Err(io::Error::new(
                io::ErrorKind::Other,
                "Failed to list boot environments",
            ));
        }

        let stdout = String::from_utf8_lossy(&output.stdout);
        let mut environments = Vec::new();

        for line in stdout.lines() {
            let parts: Vec<&str> = line.split_whitespace().collect();
            // bectl list -H columns: Name Active Mountpoint Space Created
            if parts.len() >= 5 {
                environments.push(BootEnvironment {
                    name: parts[0].to_owned(),
                    active: parts[1].contains('N') || parts[1].contains('R'),
                    mountpoint: parts[2].to_owned(),
                    creation: parts[4..].join(" "),
                });
            }
        }

        Ok(environments)
    }

    #[cfg(not(target_os = "freebsd"))]
    {
        Ok(vec![
            BootEnvironment {
                name: "default".to_owned(),
                active: true,
                mountpoint: "/".to_owned(),
                creation: "2026-11-13".to_owned(),
            },
            BootEnvironment {
                name: "backup-20261110".to_owned(),
                active: false,
                mountpoint: "-".to_owned(),
                creation: "2026-11-10".to_owned(),
            },
            BootEnvironment {
                name: "test".to_owned(),
                active: false,
                mountpoint: "-".to_owned(),
                creation: "2026-11-05".to_owned(),
            },
        ])
    }
}

pub fn activate_boot_environment(name: &str) -> io::Result<()> {
    #[cfg(target_os = "freebsd")]
    {
        let status = Command::new("/usr/sbin/bectl")
            .arg("activate")
            .arg(name)
            .status()?;

        if !status.success() {
            return Err(io::Error::new(
                io::ErrorKind::Other,
                format!("Failed to activate boot environment: {}", name),
            ));
        }

        Ok(())
    }

    #[cfg(not(target_os = "freebsd"))]
    {
        println!("Not on FreeBSD: would execute:");
        println!("  /usr/sbin/bectl activate {}", name);
        Ok(())
    }
}
