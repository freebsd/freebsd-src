use std::io;
use std::path::Path;
use std::process::Command;

#[derive(Debug, Clone)]
pub struct KernelInfo {
    pub name: String,
    pub path: String,
    pub description: String,
    pub available: bool,
}

impl KernelInfo {
    pub fn new(name: &str, path: &str, description: &str) -> Self {
        let available = Self::check_availability(path);
        Self {
            name: name.to_owned(),
            path: path.to_owned(),
            description: description.to_owned(),
            available,
        }
    }

    fn check_availability(path: &str) -> bool {
        #[cfg(target_os = "freebsd")]
        {
            Path::new(path).exists()
        }

        #[cfg(not(target_os = "freebsd"))]
        {
            // На других платформах считаем все доступными для тестирования навигации
            true
        }
    }

    pub fn get_version(&self) -> io::Result<String> {
        #[cfg(target_os = "freebsd")]
        {
            let output = Command::new("/usr/bin/what").arg(&self.path).output()?;

            if output.status.success() {
                let version = String::from_utf8_lossy(&output.stdout);
                // Ищем строку вида "@(#)FreeBSD ..."
                for line in version.lines() {
                    if line.contains("FreeBSD") {
                        return Ok(line.trim().to_owned());
                    }
                }
            }
            Ok("unknown".to_owned())
        }

        #[cfg(not(target_os = "freebsd"))]
        {
            Ok(format!("stub version for {}", self.name))
        }
    }
}

pub fn list_available_kernels() -> Vec<KernelInfo> {
    vec![
        KernelInfo::new(
            "AmeOS",
            "/boot/kernel/kernel",
            "AmeOS with customizations and patches",
        ),
        KernelInfo::new(
            "FreeBSD (vanilla)",
            "/boot/kernel.freebsd/kernel",
            "Clean FreeBSD kernel without AmeOS modifications",
        ),
        KernelInfo::new(
            "FreeBSD (old)",
            "/boot/kernel.old/kernel",
            "Previous kernel backup (if available)",
        ),
        KernelInfo::new(
            "GENERIC (safe)",
            "/boot/kernel.GENERIC/kernel",
            "Generic FreeBSD kernel for recovery",
        ),
    ]
}

pub fn get_current_kernel() -> io::Result<String> {
    #[cfg(target_os = "freebsd")]
    {
        let output = Command::new("/sbin/sysctl")
            .arg("-n")
            .arg("kern.bootfile")
            .output()?;

        if output.status.success() {
            Ok(String::from_utf8_lossy(&output.stdout).trim().to_owned())
        } else {
            Ok("unknown".to_owned())
        }
    }

    #[cfg(not(target_os = "freebsd"))]
    {
        Ok("/boot/kernel/kernel".to_owned())
    }
}
