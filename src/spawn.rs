use std::{env, ffi::CString};

use nix::sys::wait::{waitpid, WaitStatus};
use nix::unistd::{execv, fork, ForkResult};

fn main() {
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
            let args: Vec<String> = env::args().collect();

            let dir = CString::new(args[1].clone()).unwrap();
            let arg = CString::new(args[2].clone()).unwrap();

            execv(&dir, &[dir.clone(), arg]).expect("exection failed");
        }
    }
}
