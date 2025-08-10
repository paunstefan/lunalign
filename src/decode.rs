use std::path::Path;
use anyhow::Result;

pub trait Decode{
    fn decode_to_dir(input: &Path, output: &Path) -> Result<()> ;
}