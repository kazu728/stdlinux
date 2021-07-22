use std::env;

use nix::{sys::stat::Mode, unistd::mkdir};

fn main() {
    let args: Vec<String> = env::args().collect();

    if args.len() < 2 {
        panic!("No argument");
    }

    for arg in &args[1..] {
        mkdir(arg.as_str(), Mode::S_IWGRP).unwrap();
    }
}
