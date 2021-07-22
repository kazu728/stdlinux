use getopts::{Matches, Options};
use std::{env, usize};

use nix::fcntl::{open, OFlag};
use nix::sys::stat::Mode;
use nix::unistd::{read, write};

fn main() {
    let args: Vec<String> = env::args().collect();

    let (opts, matches) = parse_options(&args);

    if matches.opt_present("h") {
        print_usage(&args[0], opts);
        return;
    }

    let path = args[args.len() - 1].as_str();
    let fd = open(path, OFlag::O_RDONLY, Mode::S_IRUSR).unwrap();

    if matches.opt_present("n") {
        let n: usize = args[2].parse::<usize>().unwrap();
        head(fd, n);
    }
}

fn parse_options(args: &Vec<String>) -> (Options, Matches) {
    let mut opts = Options::new();

    opts.optflag("n", "lines", "indicate line")
        .optflag("h", "help", "print help menu");

    match opts.parse(&args[1..]) {
        Ok(m) => (opts, m),
        Err(f) => panic!("{}", f.to_string()),
    }
}

fn print_usage(program: &str, opts: Options) {
    let brief = format!("Usage: {} FILE [options]", program);
    print!("{}", opts.usage(&brief))
}

fn head(fd: i32, n: usize) {
    let mut count: usize = 1;
    let mut buffer: Vec<u8> = vec![0; 1];

    loop {
        if count > n {
            return;
        }

        match read(fd, &mut buffer) {
            Err(error) => panic!("{}", error),
            Ok(_u) => {
                write(1, &buffer).unwrap();

                if buffer[0] == 10 as u8 {
                    count += 1;
                }
            }
        }
    }
}
