use crossterm::event::KeyCode;
use std::io;

pub enum Action {
    Continue,
    Back,
    Select,
}

pub trait Screen {
    fn draw(&self) -> io::Result<()>;
    fn handle_key(&mut self, code: KeyCode) -> Action;
    fn title(&self) -> &str;
    fn selected_index(&self) -> Option<usize> {
        None
    }
}
