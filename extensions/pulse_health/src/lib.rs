use pgrx::*;

pgrx::pg_module_magic!();

#[pg_guard]
pub extern "C-unwind" fn _PG_init() {}

#[pg_extern]
pub fn run_once() -> i32 {
    0
}
