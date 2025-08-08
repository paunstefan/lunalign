use lunalign;

use lunalign::decode;
use std::path::Path;
fn main() -> Result<(),()> {
	let serfile = decode::SerFile::decode("testdata/16bit_mono.ser", Path::new("decoded"));

	println!("{:?}", serfile);	
	Ok(())
}