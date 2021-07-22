use std::ffi::CString;
use std::io::{stdin, stdout, Write};

use nix::sys::wait::{waitpid, WaitStatus};
use nix::unistd::{execv, fork, ForkResult};

// TODO: pipe, redirect
fn print_prompt() -> () {
    print!("$:");
    stdout().flush().unwrap();
}

fn main() {
    loop {
        print_prompt();

        let mut buf = String::new();
        stdin().read_line(&mut buf).unwrap();
        buf.pop();

        let input: Vec<&str> = buf.split(" ").collect();

        spawn(input[0], &input[1..].join(""));
    }
}

fn spawn(command: &str, options: &str) {
    match unsafe { fork() }.unwrap() {
        ForkResult::Parent { child } => match waitpid(child, None).expect("wait_pid failed") {
            WaitStatus::Exited(pid, status) => {
                println!("exit: pid={:?} status={:?}", pid, status)
            }
            WaitStatus::Signaled(pid, status, _) => {
                println!("signal: pid={:?} status={:?}", pid, status)
            }
            _ => println!("Invalid"),
        },
        ForkResult::Child => {
            let dir = CString::new("/bin/".to_string() + command).unwrap();
            let arg = CString::new(options).unwrap();

            execv(&dir, &[dir.clone(), arg]).expect("exection failed");
        }
    }
}
