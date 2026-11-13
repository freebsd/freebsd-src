use crossterm::{
    cursor::{Hide, MoveTo, Show},
    event::{self, Event, KeyCode, KeyEvent, KeyEventKind},
    execute,
    terminal::{self, Clear, ClearType, EnterAlternateScreen, LeaveAlternateScreen},
};
use std::io::{self, Write};

const ITEMS: &[&str] = &[
    "Boot AmeOS",
    "Boot recovery environment",
    "Hardware diagnostics",
    "Power off",
];

struct TerminalGuard;

impl TerminalGuard {
    fn enter() -> io::Result<Self> {
        terminal::enable_raw_mode()?;
        execute!(io::stdout(), EnterAlternateScreen, Hide)?;
        Ok(Self)
    }
}

impl Drop for TerminalGuard {
    fn drop(&mut self) {
        let _ = terminal::disable_raw_mode();
        let _ = execute!(io::stdout(), Show, LeaveAlternateScreen);
    }
}

fn draw(selected: usize, status: &str) -> io::Result<()> {
    let mut stdout = io::stdout();
    execute!(stdout, MoveTo(0, 0), Clear(ClearType::All))?;
    println!("AmeOS Loader");
    println!("=============");
    println!();
    println!("Select an option with Up/Down and press Enter.");
    println!("Esc exits the loader.");
    println!();
    for (index, item) in ITEMS.iter().enumerate() {
        if index == selected {
            println!("> {item}");
        } else {
            println!("  {item}");
        }
    }
    println!();
    println!("{status}");
    stdout.flush()
}

fn main() -> io::Result<()> {
    let _terminal = TerminalGuard::enter()?;
    let mut selected = 0;
    let mut status = "Ready";
    draw(selected, status)?;

    loop {
        let Event::Key(KeyEvent {
            code,
            kind: KeyEventKind::Press,
            ..
        }) = event::read()?
        else {
            continue;
        };
        match code {
            KeyCode::Up => {
                selected = selected.checked_sub(1).unwrap_or(ITEMS.len() - 1);
                status = "Ready";
                draw(selected, status)?;
            }
            KeyCode::Down => {
                selected = (selected + 1) % ITEMS.len();
                status = "Ready";
                draw(selected, status)?;
            }
            KeyCode::Enter => {
                if selected == ITEMS.len() - 1 {
                    break;
                }
                status = match selected {
                    0 => "Boot request selected (execution will be wired next)",
                    1 => "Recovery request selected (execution will be wired next)",
                    _ => "Diagnostics request selected (execution will be wired next)",
                };
                draw(selected, status)?;
            }
            KeyCode::Esc => break,
            _ => {}
        }
    }

    Ok(())
}
