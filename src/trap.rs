use nix::sys::signal::{sigaction, SaFlags, SigAction, SigHandler, SigSet, SIGINT};
use nix::unistd::pause;

extern "C" fn handle_signal(signum: i32) {
    print!("handle {}", &signum);
}
fn main() {
    let signal_action = SigAction::new(
        SigHandler::Handler(handle_signal),
        SaFlags::SA_RESETHAND,
        SigSet::empty(),
    );

    unsafe { sigaction(SIGINT, &signal_action) }.unwrap();

    pause();
}
