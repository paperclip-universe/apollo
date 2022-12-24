#![feature(c_variadic)]

mod buttons;
pub use buttons::{Buttons, InputPort};
mod emulator;
pub use emulator::Emulator;
mod error;
pub use error::*;
pub mod pixels;
