#pragma once

namespace util {

void install_signal_handlers();

[[nodiscard]] bool shutdown_requested();
void set_interrupt_socket(int fd);

} // namespace util
