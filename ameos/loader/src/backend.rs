use std::io;

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
