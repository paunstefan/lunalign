use lunalign;

use lunalign::decode::Decode;
use lunalign::ser;
use std::path::Path;


fn main() -> Result<(),()> {
	let serfile = ser::SerFile::decode_to_dir(Path::new("testdata/16bit_mono.ser"), Path::new("decoded"));

	println!("{:?}", serfile);	
	Ok(())
}