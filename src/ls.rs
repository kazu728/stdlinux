use std::env;
use std::fs;

fn main() -> () {
    let args: Vec<String> = env::args().collect();

    if args.len() < 2 {
        eprintln!("No arguemnt, {}", &args[0]);
    }
    let path = &args[1];

    ls(path);
}

fn ls(path: &str) {
    let entries = fs::read_dir(path).unwrap();

    for entry in entries {
        if let Ok(entry) = entry {
            println!("{:?}", entry.file_name());
        }
    }
}
