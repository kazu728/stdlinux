use nix::{
    fcntl::{open, OFlag},
    sys::stat::Mode,
    unistd::{close, read},
};
use std::{env, u8};

fn main() {
    let args: Vec<String> = env::args().collect();

    if args.len() < 2 {
        panic!("Invalid argument");
    }

    for path in &args[1..] {
        let fd = open(path.as_str(), OFlag::O_RDONLY, Mode::S_IRUSR).unwrap();
        wc(fd)
    }
}

fn wc(fd: i32) {
    let mut buffer: Vec<u8> = vec![0; 2048];
    let mut count = 0;

    loop {
        let u_size = read(fd, &mut buffer).unwrap();
        if u_size == 0 {
            break;
        };

        for binary in &buffer {
            if *binary == 10 {
                count = count + 1
            }
        }
    }
    println!("{}", count);
    close(fd).unwrap();
}
