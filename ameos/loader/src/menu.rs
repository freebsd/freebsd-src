use crate::screen::{Action, Screen};
use crossterm::{
    cursor::MoveTo,
    event::KeyCode,
    execute,
    terminal::{Clear, ClearType},
};
use std::io::{self, Write};

pub struct Menu {
    title: String,
    items: Vec<String>,
    selected: usize,
    status: String,
}

impl Menu {
    pub fn new(title: &str, items: Vec<String>) -> Self {
        Self {
            title: title.to_owned(),
            items,
            selected: 0,
            status: "Ready".to_owned(),
        }
    }
}

impl Screen for Menu {
    fn draw(&self) -> io::Result<()> {
        let mut stdout = io::stdout();
        execute!(stdout, MoveTo(0, 0), Clear(ClearType::All))?;
        println!("{}", self.title);
        println!("{}", "=".repeat(self.title.len()));
        println!();
        println!("Select an option with Up/Down and press Enter.");
        println!("Esc goes back or exits the loader.");
        println!();
        for (index, item) in self.items.iter().enumerate() {
            if index == self.selected {
                println!("> {item}");
            } else {
                println!("  {item}");
            }
        }
        println!();
        println!("{}", self.status);
        stdout.flush()
    }

    fn handle_key(&mut self, code: KeyCode) -> Action {
        match code {
            KeyCode::Up => {
                self.selected = self.selected.checked_sub(1).unwrap_or(self.items.len() - 1);
                self.status = "Ready".to_owned();
                Action::Continue
            }
            KeyCode::Down => {
                self.selected = (self.selected + 1) % self.items.len();
                self.status = "Ready".to_owned();
                Action::Continue
            }
            KeyCode::Enter => Action::Back,
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
