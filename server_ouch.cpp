/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#include "OUCHProtocol.hpp"
#include "server.hpp"

int main() {
  Server<OUCHProtocol> server;
  server.run();
}
