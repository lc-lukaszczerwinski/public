/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#include <iostream>

#include <signal.h>
#include <unistd.h>

#include "FIXProtocol.hpp"
#include "client.hpp"

namespace {

void handle_sigint(int) {
  constexpr char message[] = /* YELLOW_PASTEL */ "\033[38;5;222m"
                                                 "\nType "
                                                 /* ITALIC     */ "\033[3m"
                                                 "exit"
                                                 /* RESET ITALIC */ "\033[23m"
                                                 " to quit."
                                                 /* RESET YELLOW  */ "\033[0m"
                                                 "\n> ";
  (void)::write(STDERR_FILENO, message, sizeof(message) - 1);
}

} // namespace

int main() {
  struct sigaction action{};
  action.sa_handler = handle_sigint;
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_RESTART;

  if (sigaction(SIGINT, &action, nullptr) != 0) {
    std::cerr << "Unable to install SIGINT handler.\n";
    return 1;
  }

  Client<FIXProtocol> client;
  client.run();
}
