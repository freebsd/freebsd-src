use crossterm::style::Color;

pub struct Theme {
    pub border: Color,
    pub title: Color,
    pub selected: Color,
    pub normal: Color,
    pub status: Color,
    #[allow(dead_code)]
    pub background: Color,
}

impl Theme {
    pub fn ameos() -> Self {
        Self {
            border: Color::Cyan,
            title: Color::White,
            selected: Color::Yellow,
            normal: Color::White,
            status: Color::Green,
            background: Color::Black,
        }
    }
}

pub const LOGO: &str = r#"
    ___                  ____  _____
   /   |  ____ ___  ___ / __ \/ ___/
  / /| | / __ `__ \/ _ \ / / /\__ \ 
 / ___ |/ / / / / /  __/ /_/ /___/ / 
/_/  |_/_/ /_/ /_/\___/\____//____/  
"#;
