use std::{env, ffi::CString, fs::File, io::Read, os::unix::io, ptr};

use nix::libc::{
    addrinfo, close, connect, freeaddrinfo, gai_strerror, getaddrinfo, socket, AF_UNSPEC,
    SOCK_STREAM,
};

fn main() {
    let args: Vec<String> = env::args().collect();

    if args.len() < 2 {
        panic!("Invalid args");
    }
    let arg: String = args.get(1).unwrap().to_string();

    let socket: io::RawFd = open_connection(
        CString::new(arg).unwrap().as_ptr(),
        CString::new("daytime").unwrap().as_ptr(),
    );

    let mut f: File = unsafe { io::FromRawFd::from_raw_fd(socket) };
    let mut contents = String::new();

    f.read_to_string(&mut contents)
        .expect("failed to read contents");

    print!("{}", contents);
}

fn open_connection(host: *const i8, service: *const i8) -> i32 {
    let hints: addrinfo = default_addrinfo();

    let mut res: *mut addrinfo = default_mut_addrinfo();

    let err = unsafe { getaddrinfo(host, service, &hints, &mut res) };

    println!("アドレス {:?}", unsafe { *(*res).ai_addr });

    if err != 0 {
        eprintln!("getaddrinfo(3): {:?}", unsafe { gai_strerror(err) });
        return 0;
    }

    while !res.is_null() {
        let sock = unsafe { socket((*res).ai_family, (*res).ai_socktype, (*res).ai_protocol) };

        if sock < 0 {
            continue;
        }
        let con = unsafe { connect(sock, (*res).ai_addr, (*res).ai_addrlen) };

        if con < 0 {
            unsafe { close(sock) };
            continue;
        }
        res = unsafe { (*res).ai_next };

        if res.is_null() {
            unsafe { freeaddrinfo(res) };
            return sock;
        }
    }
    unsafe { freeaddrinfo(res) };

    std::process::exit(1);
}

fn default_addrinfo() -> addrinfo {
    addrinfo {
        ai_flags: 0,
        ai_family: AF_UNSPEC,
        ai_socktype: SOCK_STREAM,
        ai_protocol: 0,
        ai_addrlen: 0,
        ai_canonname: ptr::null_mut(),
        ai_addr: ptr::null_mut(),
        ai_next: ptr::null_mut(),
    }
}

fn default_mut_addrinfo() -> *mut addrinfo {
    Box::new(default_addrinfo()).as_mut()
}
