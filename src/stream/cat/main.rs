use nix::fcntl::{open, OFlag};
use nix::sys::stat::Mode;
use nix::unistd::{close, read, write};
use std::process::exit;
use std::{env, str};

fn main() -> () {
    let args: Vec<String> = env::args().collect();

    if args.len() < 2 {
        const STD_INPUT: i32 = 0;
        cat(STD_INPUT);
        return;
    }

    for arg in &args[1..] {
        let fd = get_fd(arg);
        cat(fd)
    }
}

fn get_fd(path: &str) -> i32 {
    let fd = open(path, OFlag::O_RDONLY, Mode::S_IRUSR).unwrap();

    if fd < 0 {
        eprintln!("Could not opent the file");
        exit(1);
    }
    fd
}

fn cat(fd: i32) -> () {
    let mut buffer: Vec<u8> = vec![0; 2048];

    loop {
        match read(fd, &mut buffer).unwrap() {
            0 => break,
            _ => {
                write(1, &buffer).unwrap();
            }
        }
    }
    close(fd).unwrap();
}