use lunalign;

use anyhow::Result;
use lunalign::decode::Decode;
use lunalign::ser;
use std::fs;
use std::path::Path;

use clap::{Args, Parser};

#[derive(Parser, Debug)]
#[clap(author = "Stefan Paun", version = "1.0", about, long_about = None)]
struct Config {
    #[arg(short, long)]
    input: String,
    // #[arg(short, long)]
    // output: String,
}

fn main() -> Result<()> {
    let config = Config::parse();

    fs::create_dir_all("./process/input_fits");

    let input_data = Path::new(&config.input);

    if input_data.is_file() && input_data.extension().unwrap() == "ser" {
        let serfile =
            ser::SerFile::decode_to_dir(input_data, Path::new("./process/input_fits"));
    }
    Ok(())
}
