use std::io;
#[cfg(target_os = "freebsd")]
use std::process::Command;

pub trait BootBackend {
    fn boot_default(&self) -> io::Result<()>;
    fn boot_single_user(&self) -> io::Result<()>;
    fn boot_safe_mode(&self) -> io::Result<()>;
    fn mount_filesystems(&self) -> io::Result<()>;
    fn configure_network(&self) -> io::Result<()>;
    fn spawn_shell(&self) -> io::Result<()>;
    fn run_memory_test(&self) -> io::Result<()>;
    fn run_disk_test(&self) -> io::Result<()>;
    fn show_cpu_info(&self) -> io::Result<()>;
}

pub struct StubBackend;

impl BootBackend for StubBackend {
    fn boot_default(&self) -> io::Result<()> {
        println!("Stub: would boot AmeOS with default profile");
        Ok(())
    }

    fn boot_single_user(&self) -> io::Result<()> {
        println!("Stub: would boot AmeOS in single user mode");
        Ok(())
    }

    fn boot_safe_mode(&self) -> io::Result<()> {
        println!("Stub: would boot AmeOS in safe mode");
        Ok(())
    }

    fn mount_filesystems(&self) -> io::Result<()> {
        println!("Stub: would mount filesystems");
        Ok(())
    }

    fn configure_network(&self) -> io::Result<()> {
        println!("Stub: would configure network");
        Ok(())
    }

    fn spawn_shell(&self) -> io::Result<()> {
        println!("Stub: would spawn recovery shell");
        Ok(())
    }

    fn run_memory_test(&self) -> io::Result<()> {
        println!("Stub: would run memory diagnostics");
        Ok(())
    }

    fn run_disk_test(&self) -> io::Result<()> {
        println!("Stub: would run disk diagnostics");
        Ok(())
    }

    fn show_cpu_info(&self) -> io::Result<()> {
        println!("Stub: would show CPU information");
        Ok(())
    }
}

#[cfg_attr(not(target_os = "freebsd"), allow(dead_code))]
pub struct FreeBSDBackend {
    kernel_path: String,
}

#[cfg_attr(not(target_os = "freebsd"), allow(dead_code))]
impl FreeBSDBackend {
    pub fn new() -> Self {
        Self {
            kernel_path: "/boot/kernel/kernel".to_owned(),
        }
    }

    pub fn with_kernel(kernel_path: String) -> Self {
        Self { kernel_path }
    }

    pub fn set_kernel(&mut self, kernel_path: String) {
        self.kernel_path = kernel_path;
    }

    pub fn kernel_path(&self) -> &str {
        &self.kernel_path
    }

    fn boot_with_flags(&self, flags: &str) -> io::Result<()> {
        println!("Booting FreeBSD kernel: {}", self.kernel_path);
        println!("Boot flags: {}", flags);

        #[cfg(target_os = "freebsd")]
        {
            use std::process;
            let status = Command::new("/sbin/reboot")
                .arg("-k")
                .arg(&self.kernel_path)
                .arg(flags)
                .status()?;

            if !status.success() {
                return Err(io::Error::new(
                    io::ErrorKind::Other,
                    "Failed to execute reboot command",
                ));
            }
            process::exit(0);
        }

        #[cfg(not(target_os = "freebsd"))]
        {
            println!("Not on FreeBSD: boot command would be:");
            println!("  /sbin/reboot -k {} {}", self.kernel_path, flags);
            Err(io::Error::new(
                io::ErrorKind::Unsupported,
                "FreeBSD boot is only available on FreeBSD systems",
            ))
        }
    }
}

impl Default for FreeBSDBackend {
    fn default() -> Self {
        Self::new()
    }
}

impl BootBackend for FreeBSDBackend {
    fn boot_default(&self) -> io::Result<()> {
        self.boot_with_flags("")
    }

    fn boot_single_user(&self) -> io::Result<()> {
        self.boot_with_flags("-s")
    }

    fn boot_safe_mode(&self) -> io::Result<()> {
        self.boot_with_flags("-v")
    }

    fn mount_filesystems(&self) -> io::Result<()> {
        println!("Mounting root filesystem read-write...");

        #[cfg(target_os = "freebsd")]
        {
            let status = Command::new("/sbin/mount")
                .arg("-u")
                .arg("-w")
                .arg("/")
                .status()?;

            if !status.success() {
                return Err(io::Error::new(
                    io::ErrorKind::Other,
                    "Failed to remount root filesystem",
                ));
            }

            println!("Root filesystem mounted read-write");
            println!("Mounting all filesystems from fstab...");

            let status = Command::new("/sbin/mount").arg("-a").status()?;

            if !status.success() {
                return Err(io::Error::new(
                    io::ErrorKind::Other,
                    "Failed to mount filesystems",
                ));
            }

            println!("All filesystems mounted");
        }

        #[cfg(not(target_os = "freebsd"))]
        {
            println!("Not on FreeBSD: would execute:");
            println!("  /sbin/mount -u -w /");
            println!("  /sbin/mount -a");
        }

        Ok(())
    }

    fn configure_network(&self) -> io::Result<()> {
        println!("Starting network configuration...");

        #[cfg(target_os = "freebsd")]
        {
            let status = Command::new("/sbin/dhclient").arg("-b").status()?;

            if !status.success() {
                println!("DHCP configuration failed, entering manual mode");
            } else {
                println!("Network configured via DHCP");
            }
        }

        #[cfg(not(target_os = "freebsd"))]
        {
            println!("Not on FreeBSD: would execute:");
            println!("  /sbin/dhclient -b");
        }

        Ok(())
    }

    fn spawn_shell(&self) -> io::Result<()> {
        println!("Spawning recovery shell...");
        println!("Type 'exit' to return to the loader");

        #[cfg(target_os = "freebsd")]
        {
            let status = Command::new("/bin/sh").status()?;

            if !status.success() {
                return Err(io::Error::new(
                    io::ErrorKind::Other,
                    "Shell exited with error",
                ));
            }
        }

        #[cfg(not(target_os = "freebsd"))]
        {
            println!("Not on FreeBSD: would spawn /bin/sh");
            println!("Press Enter to continue...");
            let mut input = String::new();
            io::stdin().read_line(&mut input)?;
        }

        Ok(())
    }

    fn run_memory_test(&self) -> io::Result<()> {
        println!("Running memory diagnostics...");

        #[cfg(target_os = "freebsd")]
        {
            let output = Command::new("/sbin/sysctl")
                .arg("hw.physmem")
                .arg("hw.usermem")
                .arg("hw.realmem")
                .output()?;

            if output.status.success() {
                println!("{}", String::from_utf8_lossy(&output.stdout));
            } else {
                return Err(io::Error::new(
                    io::ErrorKind::Other,
                    "Failed to read memory information",
                ));
            }
        }

        #[cfg(not(target_os = "freebsd"))]
        {
            println!("Not on FreeBSD: would execute:");
            println!("  /sbin/sysctl hw.physmem hw.usermem hw.realmem");
            println!("\nSample output:");
            println!("hw.physmem: 8589934592");
            println!("hw.usermem: 7516192768");
            println!("hw.realmem: 8589934592");
        }

        println!("\nPress Enter to continue...");
        let mut input = String::new();
        io::stdin().read_line(&mut input)?;

        Ok(())
    }

    fn run_disk_test(&self) -> io::Result<()> {
        println!("Running disk diagnostics...");

        #[cfg(target_os = "freebsd")]
        {
            let output = Command::new("/sbin/geom")
                .arg("disk")
                .arg("list")
                .output()?;

            if output.status.success() {
                println!("{}", String::from_utf8_lossy(&output.stdout));
            } else {
                return Err(io::Error::new(
                    io::ErrorKind::Other,
                    "Failed to list disk devices",
                ));
            }
        }

        #[cfg(not(target_os = "freebsd"))]
        {
            println!("Not on FreeBSD: would execute:");
            println!("  /sbin/geom disk list");
            println!("\nSample output:");
            println!("Geom name: ada0");
            println!("Providers:");
            println!("1. Name: ada0");
            println!("   Mediasize: 512110190592 (477G)");
            println!("   Sectorsize: 512");
        }

        println!("\nPress Enter to continue...");
        let mut input = String::new();
        io::stdin().read_line(&mut input)?;

        Ok(())
    }

    fn show_cpu_info(&self) -> io::Result<()> {
        println!("CPU Information:");

        #[cfg(target_os = "freebsd")]
        {
            let output = Command::new("/sbin/sysctl")
                .arg("hw.model")
                .arg("hw.ncpu")
                .arg("hw.machine")
                .arg("hw.machine_arch")
                .output()?;

            if output.status.success() {
                println!("{}", String::from_utf8_lossy(&output.stdout));
            } else {
                return Err(io::Error::new(
                    io::ErrorKind::Other,
                    "Failed to read CPU information",
                ));
            }
        }

        #[cfg(not(target_os = "freebsd"))]
        {
            println!("Not on FreeBSD: would execute:");
            println!("  /sbin/sysctl hw.model hw.ncpu hw.machine hw.machine_arch");
            println!("\nSample output:");
            println!("hw.model: Intel(R) Core(TM) i7-9700K CPU @ 3.60GHz");
            println!("hw.ncpu: 8");
            println!("hw.machine: amd64");
            println!("hw.machine_arch: amd64");
        }

        println!("\nPress Enter to continue...");
        let mut input = String::new();
        io::stdin().read_line(&mut input)?;

        Ok(())
    }
}
