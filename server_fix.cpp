/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#include "FIXProtocol.hpp"
#include "server.hpp"

int main() {
  Server<FIXProtocol> server;
  server.run();
}
