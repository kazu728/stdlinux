use regex::Regex;
use std::io::{BufRead, BufReader};
use std::{env, fs::File};

fn main() {
    let args: Vec<String> = env::args().collect();

    if args.len() < 2 {}

    let (regex, path) = (&args[1], &args[2]);

    grep(regex, File::open(path).unwrap());
}

fn grep(pattern: &str, file: File) -> () {
    let reader = BufReader::new(file);

    for maybe_line in reader.lines() {
        let line = maybe_line.unwrap();

        let regex = Regex::new(&format!(r"{}", pattern)).unwrap();

        if regex.is_match(&line) {
            println!("{}", line);
        }
    }
}