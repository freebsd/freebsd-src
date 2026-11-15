use crate::screen::{Action, Screen};
use crate::theme::{Theme, LOGO};
use crossterm::{
    cursor::MoveTo,
    event::KeyCode,
    execute,
    style::{Color, Print, ResetColor, SetForegroundColor},
    terminal::{Clear, ClearType},
};
use std::io::{self, Write};

pub struct Menu {
    title: String,
    items: Vec<String>,
    selected: usize,
    status: String,
    theme: Theme,
    show_logo: bool,
}

impl Menu {
    pub fn new(title: &str, items: Vec<String>) -> Self {
        let show_logo = title == "AmeOS Loader";
        Self {
            title: title.to_owned(),
            items,
            selected: 0,
            status: "Ready".to_owned(),
            theme: Theme::ameos(),
            show_logo,
        }
    }
}

impl Screen for Menu {
    fn draw(&self) -> io::Result<()> {
        let mut stdout = io::stdout();
        execute!(stdout, MoveTo(0, 0), Clear(ClearType::All))?;

        let mut line = 0;

        if self.show_logo {
            execute!(stdout, SetForegroundColor(self.theme.border))?;
            for logo_line in LOGO.lines() {
                execute!(stdout, MoveTo(0, line), Print(logo_line))?;
                line += 1;
            }
            execute!(stdout, ResetColor)?;
            line += 1;
        }

        execute!(
            stdout,
            MoveTo(0, line),
            SetForegroundColor(self.theme.border),
            Print("╔"),
            Print("═".repeat(self.title.len() + 2)),
            Print("╗"),
            ResetColor
        )?;
        line += 1;

        execute!(
            stdout,
            MoveTo(0, line),
            SetForegroundColor(self.theme.border),
            Print("║ "),
            SetForegroundColor(self.theme.title),
            Print(&self.title),
            SetForegroundColor(self.theme.border),
            Print(" ║"),
            ResetColor
        )?;
        line += 1;

        execute!(
            stdout,
            MoveTo(0, line),
            SetForegroundColor(self.theme.border),
            Print("╚"),
            Print("═".repeat(self.title.len() + 2)),
            Print("╝"),
            ResetColor
        )?;
        line += 2;

        execute!(
            stdout,
            MoveTo(0, line),
            SetForegroundColor(Color::DarkGrey),
            Print("Use ↑/↓ to navigate, Enter to select, Esc to go back"),
            ResetColor
        )?;
        line += 2;

        for (index, item) in self.items.iter().enumerate() {
            if index == self.selected {
                execute!(
                    stdout,
                    MoveTo(0, line),
                    SetForegroundColor(self.theme.selected),
                    Print("► "),
                    Print(item),
                    ResetColor
                )?;
            } else {
                execute!(
                    stdout,
                    MoveTo(0, line),
                    SetForegroundColor(self.theme.normal),
                    Print("  "),
                    Print(item),
                    ResetColor
                )?;
            }
            line += 1;
        }

        line += 1;
        execute!(
            stdout,
            MoveTo(0, line),
            SetForegroundColor(self.theme.status),
            Print(&self.status),
            ResetColor
        )?;

        stdout.flush()
    }

    fn handle_key(&mut self, code: KeyCode) -> Action {
        match code {
            KeyCode::Up => {
                if self.items.is_empty() {
                    return Action::Continue;
                }
                self.selected = self.selected.checked_sub(1).unwrap_or(self.items.len() - 1);
                self.status = "Ready".to_owned();
                Action::Continue
            }
            KeyCode::Down => {
                if self.items.is_empty() {
                    return Action::Continue;
                }
                self.selected = (self.selected + 1) % self.items.len();
                self.status = "Ready".to_owned();
                Action::Continue
            }
            KeyCode::Enter => Action::Select,
            KeyCode::Esc => Action::Back,
            _ => Action::Continue,
        }
    }

    fn title(&self) -> &str {
        &self.title
    }

    fn selected_index(&self) -> Option<usize> {
        Some(self.selected)
    }
}
