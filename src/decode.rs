use opencv::prelude::*;

pub struct Ser {
    pub name: String,
    pub color: i32,
    pub endianess: i32,
    pub width: i32,
    pub height: i32,
    pub pixel_depth: i32,
    pub frame_count: i32,
}