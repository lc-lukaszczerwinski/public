
#include <format>
#include <sstream>
#include <string>

namespace FIX {

constexpr int ClOrdId = 11;
constexpr int CumQty = 14;
constexpr int LastPrice = 31;
constexpr int LastQty = 32;
constexpr int MsgSeqNum = 34;
constexpr int MsgType = 35;
constexpr int OrderId = 37;
constexpr int Quantity = 38;
constexpr int OrdStatus = 39;
constexpr int OrdType = 40;
constexpr int OrderPrice = 44;
constexpr int SenderCompId = 49;
constexpr int Side = 54;
constexpr int TargetCompId = 56;
constexpr int TimeInForce = 59;
constexpr int ExecType = 150;
constexpr int LeavesQty = 151;

constexpr char NewOrderSingle = 'D';
constexpr char ExecutionReport = '8';

constexpr char ExecType_New = '0';
constexpr char ExecType_Canceled = '4';
constexpr char ExecType_Replaced = '5';
constexpr char ExecType_Rejected = '8';
constexpr char ExecType_Trade = 'F';

constexpr char OrdStatus_New = '0';
constexpr char OrdStatus_PartiallyFilled = '1';
constexpr char OrdStatus_Filled = '2';
constexpr char OrdStatus_Canceled = '4';
constexpr char OrdStatus_Rejected = '8';

constexpr char Buy = '1';
constexpr char Sell = '2';

constexpr char Market = '1';
constexpr char Limit = '2';

constexpr char Day = '0';
constexpr char Ioc = '3';

template<typename T>
std::string tag(int32_t tag, const T& value) {
  return std::format("{}={}|", tag, value);
}

std::string tag(std::string& fix, int expected_tag) {
  auto eq = fix.find('=');
  assert(eq != std::string::npos);

  auto sep = fix.find('|');
  assert(sep != std::string::npos);

  int actual_tag = std::stoi(fix.substr(0, eq));
  assert(actual_tag == expected_tag);

  std::string value = fix.substr(eq + 1, sep - eq - 1);
  fix.erase(0, sep + 1);

  return value;
}

int32_t stoi(const std::string& s) {
  return std::stoi(s);
};

int64_t stol(const std::string& s) {
  return std::stoll(s);
};

char stoc(const std::string& s) {
  return s[0];
};

} // namespace FIX
