use fitsio::FitsFile;
use fitsio::images::{ImageDescription, ImageType};
use std::fs::File;
use std::io::{BufReader, Read};
use std::path::Path;
use anyhow::Result;
use crate::decode::Decode;

#[derive(Debug, Clone)]
pub struct SerFile {
    header: SerHeader,
}

#[derive(Debug, Clone)]
pub struct SerHeader {
    pub name: String,
    pub color: i32,
    pub endianess: i32,
    pub width: i32,
    pub height: i32,
    pub pixel_depth: i32,
    pub frame_count: i32,
}

impl Decode for SerFile {
    fn decode_to_dir(input: &Path, output: &Path) -> Result<()> {
        let f = File::open(input)?;
        let mut reader = BufReader::new(f);

        let mut header_buffer = [0u8; 178];
        reader.read_exact(&mut header_buffer)?;

        let color = i32::from_le_bytes(header_buffer[18..22].try_into()?);
        let endianess = i32::from_le_bytes(header_buffer[22..26].try_into()?);
        let width = i32::from_le_bytes(header_buffer[26..30].try_into()?);
        let height = i32::from_le_bytes(header_buffer[30..34].try_into()?);
        let pixel_depth = i32::from_le_bytes(header_buffer[34..38].try_into()?);
        let frame_count = i32::from_le_bytes(header_buffer[38..42].try_into()?);
        // The other fields are not relevant for the moment

        let header = SerHeader {
            name: input.display().to_string(),
            color,
            endianess,
            width,
            height,
            pixel_depth,
            frame_count,
        };

        println!("{:?}", header);

        let pixels_per_frame = (header.width * header.height) as usize;
        let bytes_per_pixel = (header.pixel_depth / 8) as usize;
        let frame_size_bytes = pixels_per_frame * bytes_per_pixel;

        let image_type = match header.pixel_depth {
            8 => ImageType::UnsignedByte,
            16 => ImageType::UnsignedShort,
            32 => ImageType::UnsignedLong,
            _ => {
                anyhow::bail!("Unsupported pixel depth {}", header.pixel_depth);
            }
        };


        for i in 0..header.frame_count {
            println!("Processing frame {}/{}", i, header.frame_count);
            let mut frame_buffer = vec![0u8; frame_size_bytes];
            reader.read_exact(&mut frame_buffer)?;
            let output_filename = output.join(format!("decoded_{:04}.fits", i));
            println!("{:?}", output_filename);

            let description = ImageDescription {
                data_type: image_type,
                dimensions: &[header.height as usize, header.width as usize],
            };

            let mut fptr = if output_filename.exists() {
                FitsFile::edit(&output_filename)?
            } else {
                FitsFile::create(&output_filename)
                    .with_custom_primary(&description)
                    .open()?
            };
            let hdu = fptr.primary_hdu()?;

            match image_type {
                ImageType::UnsignedByte => {
                    // The buffer is already Vec<u8>, so we can use it directly.
                    hdu.write_image(&mut fptr, &frame_buffer)?;
                }
                ImageType::UnsignedShort => {
                    // Convert the Vec<u8> into a Vec<u16>.
                    let image_data: Vec<u16> = frame_buffer
                        .chunks_exact(2)
                        .map(|chunk| u16::from_le_bytes([chunk[0], chunk[1]]))
                        .collect();
                    hdu.write_image(&mut fptr, &image_data)?;
                }
                ImageType::UnsignedLong => {
                    // Convert the Vec<u8> into a Vec<u32>.
                    let image_data: Vec<u32> = frame_buffer
                        .chunks_exact(4)
                        .map(|chunk| u32::from_le_bytes([chunk[0], chunk[1], chunk[2], chunk[3]]))
                        .collect();
                    hdu.write_image(&mut fptr, &image_data)?;
                }
                _ => unreachable!(),
            }
        }
        println!(
            "\nConversion complete! {} FITS files written to '{}'.",
            header.frame_count,
            output.display()
        );
        Ok(())
    }
}
