
#include <format>
#include <sstream>
#include <string>

namespace FIX {

constexpr int ClOrdId_11 = 11;
constexpr int CumQty_14 = 14;
constexpr int LastPrice_31 = 31;
constexpr int LastQty_32 = 32;
constexpr int MsgSeqNum_34 = 34;
constexpr int MsgType_35 = 35;
constexpr int OrderId_37 = 37;
constexpr int Quantity_38 = 38;
constexpr int OrdStatus_39 = 39;
constexpr int OrdType_40 = 40;
constexpr int OrigClOrdId_41 = 41;
constexpr int OrderPrice_44 = 44;
constexpr int SenderCompId_49 = 49;
constexpr int Side_54 = 54;
constexpr int TargetCompId_56 = 56;
constexpr int TimeInForce_59 = 59;
constexpr int ExecType_150 = 150;
constexpr int LeavesQty_151 = 151;

constexpr char MsgType_NewOrderSingle = 'D';
constexpr char MsgType_ExecutionReport = '8';
constexpr char MsgType_OrderCancelRequest = 'F';
constexpr char MsgType_CancelRejected = '9';

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

constexpr char OrdType_Market = '1';
constexpr char OrdType_Limit = '2';

constexpr char TimeInForce_Day = '0';
constexpr char TimeInForce_GTC = '1';
constexpr char TimeInForce_IOC = '3';

template<typename T>
std::string tag(int32_t tag, const T& value) {
  return std::format("{}={}|", tag, value);
}

inline std::string tag(std::string& fix, int expected_tag) {
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

inline int32_t stoi(const std::string& s) {
  return std::stoi(s);
};

inline int64_t stol(const std::string& s) {
  return std::stoll(s);
};

inline char stoc(const std::string& s) {
  return s[0];
};

} // namespace FIX
