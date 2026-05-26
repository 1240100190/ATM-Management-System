#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <limits>

using namespace std;

string getCurrentDateTime()
{
    time_t now = time(0);
    char buf[80];
    struct tm tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%d-%m-%Y %H:%M", &tstruct);
    return string(buf);
}

class Transaction
{
public:
    string type;
    double amount;
    double balanceAfter;
    string dateTime;

    Transaction(string t, double amt, double bal)
        : type(t), amount(amt), balanceAfter(bal), dateTime(getCurrentDateTime()) {}

    Transaction(string t, double amt, double bal, string dt)
        : type(t), amount(amt), balanceAfter(bal), dateTime(dt) {}

    string serialize() const
    {
        return type + "|" + to_string(amount) + "|" + to_string(balanceAfter) + "|" + dateTime;
    }

    static Transaction deserialize(const string& line)
    {
        stringstream ss(line);
        string type, amtStr, balStr, dt;
        getline(ss, type, '|');
        getline(ss, amtStr, '|');
        getline(ss, balStr, '|');
        getline(ss, dt);
        return Transaction(type, stod(amtStr), stod(balStr), dt);
    }

    void display() const
    {
        cout << "  " << left << setw(12) << type
             << "PKR " << setw(12) << fixed << setprecision(2) << amount
             << "Balance: PKR " << fixed << setprecision(2) << balanceAfter
             << "  [" << dateTime << "]" << endl;
    }
};

class Account
{
protected:
    string accountNumber;
    string holderName;
    string pin;
    double balance;
    string accountType;
    vector<Transaction> transactions;

public:
    Account(string accNo, string name, string pin, double balance, string type)
        : accountNumber(accNo), holderName(name), pin(pin), balance(balance), accountType(type) {}

    virtual ~Account() {}

    string getAccountNumber() const { return accountNumber; }
    string getHolderName() const { return holderName; }
    string getAccountType() const { return accountType; }
    double getBalance() const { return balance; }

    bool verifyPin(const string& inputPin) const { return pin == inputPin; }

    bool changePin(const string& oldPin, const string& newPin)
    {
        if (!verifyPin(oldPin)) return false;
        if (newPin.length() != 4) return false;
        for (char c : newPin)
            if (!isdigit(c)) return false;
        pin = newPin;
        return true;
    }

    virtual bool deposit(double amount)
    {
        if (amount <= 0) return false;
        balance += amount;
        transactions.push_back(Transaction("DEPOSIT", amount, balance));
        return true;
    }

    virtual bool withdraw(double amount) = 0;

    virtual double getDailyLimit() const = 0;

    void showTransactionHistory() const
    {
        if (transactions.empty())
        {
            cout << "\n  No transactions recorded." << endl;
            return;
        }
        cout << "\n  ── Transaction History (" << transactions.size() << " records) ──" << endl;
        for (const auto& t : transactions) t.display();
    }

    void displayInfo() const
    {
        cout << "\n  ╔══════════════════════════════════════════╗" << endl;
        cout << "  ║           Account Information            ║" << endl;
        cout << "  ╚══════════════════════════════════════════╝" << endl;
        cout << "  Account No   : " << accountNumber << endl;
        cout << "  Holder Name  : " << holderName << endl;
        cout << "  Account Type : " << accountType << endl;
        cout << "  Balance      : PKR " << fixed << setprecision(2) << balance << endl;
        cout << "  Daily Limit  : PKR " << fixed << setprecision(2) << getDailyLimit() << endl;
    }

    string getPin() const { return pin; }

    void addTransaction(const Transaction& t) { transactions.push_back(t); }

    vector<Transaction>& getTransactions() { return transactions; }

    virtual string serialize() const
    {
        return accountNumber + "|" + holderName + "|" + pin + "|" +
               to_string(balance) + "|" + accountType;
    }
};

class SavingsAccount : public Account
{
    double interestRate;

public:
    SavingsAccount(string accNo, string name, string pin, double balance)
        : Account(accNo, name, pin, balance, "SAVINGS"), interestRate(0.05) {}

    double getDailyLimit() const override { return 50000.0; }

    bool withdraw(double amount) override
    {
        if (amount <= 0)
        {
            cout << "\n  Invalid amount." << endl;
            return false;
        }
        if (amount > balance)
        {
            cout << "\n  Insufficient balance. Available: PKR "
                 << fixed << setprecision(2) << balance << endl;
            return false;
        }
        if (amount > getDailyLimit())
        {
            cout << "\n  Amount exceeds daily limit of PKR "
                 << fixed << setprecision(2) << getDailyLimit()
                 << " for Savings accounts." << endl;
            return false;
        }
        balance -= amount;
        transactions.push_back(Transaction("WITHDRAWAL", amount, balance));
        return true;
    }

    void applyMonthlyInterest()
    {
        double interest = balance * interestRate / 12;
        balance += interest;
        transactions.push_back(Transaction("INTEREST", interest, balance));
        cout << "\n  Interest applied: PKR " << fixed << setprecision(2) << interest << endl;
    }
};

class CurrentAccount : public Account
{
    double overdraftLimit;

public:
    CurrentAccount(string accNo, string name, string pin, double balance)
        : Account(accNo, name, pin, balance, "CURRENT"), overdraftLimit(10000.0) {}

    double getDailyLimit() const override { return 200000.0; }

    bool withdraw(double amount) override
    {
        if (amount <= 0)
        {
            cout << "\n  Invalid amount." << endl;
            return false;
        }
        if (amount > balance + overdraftLimit)
        {
            cout << "\n  Amount exceeds available balance including overdraft of PKR "
                 << fixed << setprecision(2) << overdraftLimit << endl;
            return false;
        }
        if (amount > getDailyLimit())
        {
            cout << "\n  Amount exceeds daily limit of PKR "
                 << fixed << setprecision(2) << getDailyLimit()
                 << " for Current accounts." << endl;
            return false;
        }
        balance -= amount;
        transactions.push_back(Transaction("WITHDRAWAL", amount, balance));
        return true;
    }
};

class AccountManager
{
    vector<Account*> accounts;
    const string dataFile = "accounts.txt";
    int loginAttempts = 0;
    const int maxAttempts = 3;

    void loadAccounts()
    {
        ifstream file(dataFile);
        if (!file.is_open()) return;
        string line;
        while (getline(file, line))
        {
            if (line.empty()) continue;
            stringstream ss(line);
            string accNo, name, pin, balStr, type;
            getline(ss, accNo, '|');
            getline(ss, name, '|');
            getline(ss, pin, '|');
            getline(ss, balStr, '|');
            getline(ss, type);

            double bal = stod(balStr);
            Account* acc = nullptr;
            if (type == "SAVINGS")
                acc = new SavingsAccount(accNo, name, pin, bal);
            else
                acc = new CurrentAccount(accNo, name, pin, bal);
            accounts.push_back(acc);
        }
        file.close();
    }

    void saveAccounts()
    {
        ofstream file(dataFile);
        for (const auto& acc : accounts)
            file << acc->serialize() << "\n";
        file.close();
    }

public:
    AccountManager()
    {
        loadAccounts();
        if (accounts.empty())
        {
            accounts.push_back(new SavingsAccount("SA-0001", "Ali Hassan", "1234", 75000.0));
            accounts.push_back(new CurrentAccount("CA-0001", "Sara Malik", "4321", 200000.0));
            accounts.push_back(new SavingsAccount("SA-0002", "Ahmed Khan", "1111", 15000.0));
            saveAccounts();
        }
    }

    ~AccountManager()
    {
        saveAccounts();
        for (auto acc : accounts) delete acc;
    }

    Account* authenticate(const string& accNo, const string& pin)
    {
        for (auto acc : accounts)
        {
            if (acc->getAccountNumber() == accNo)
            {
                if (acc->verifyPin(pin))
                {
                    loginAttempts = 0;
                    return acc;
                }
                else
                {
                    loginAttempts++;
                    return nullptr;
                }
            }
        }
        return nullptr;
    }

    bool isLocked() const { return loginAttempts >= maxAttempts; }

    int getRemainingAttempts() const { return maxAttempts - loginAttempts; }

    void saveState() { saveAccounts(); }
};

class ATMInterface
{
    AccountManager manager;

    string readHiddenPin(const string& prompt)
    {
        cout << "  " << prompt << ": ";
        string pin;
        cin >> pin;
        return pin;
    }

    double readAmount(const string& prompt)
    {
        double amount;
        while (true)
        {
            cout << "  " << prompt << ": PKR ";
            if (cin >> amount && amount > 0) return amount;
            cout << "\n  Invalid amount. Please enter a positive number." << endl;
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
    }

    void showAccountMenu(Account* acc)
    {
        bool loggedIn = true;
        while (loggedIn)
        {
            cout << "\n  ── Account Menu ───────────────────────────" << endl;
            cout << "  [1]  Balance Inquiry" << endl;
            cout << "  [2]  Deposit" << endl;
            cout << "  [3]  Withdraw" << endl;
            cout << "  [4]  Change PIN" << endl;
            cout << "  [5]  Transaction History" << endl;
            cout << "  [0]  Logout" << endl;
            cout << "\n  Choice: ";

            int choice;
            if (!(cin >> choice))
            {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                continue;
            }

            switch (choice)
            {
                case 1:
                    acc->displayInfo();
                    break;

                case 2:
                {
                    double amount = readAmount("Enter deposit amount");
                    if (acc->deposit(amount))
                    {
                        manager.saveState();
                        cout << "\n  Deposit successful." << endl;
                        cout << "  New Balance: PKR " << fixed << setprecision(2)
                             << acc->getBalance() << endl;
                    }
                    break;
                }

                case 3:
                {
                    double amount = readAmount("Enter withdrawal amount");
                    if (acc->withdraw(amount))
                    {
                        manager.saveState();
                        cout << "\n  Withdrawal successful." << endl;
                        cout << "  Remaining Balance: PKR " << fixed << setprecision(2)
                             << acc->getBalance() << endl;
                    }
                    break;
                }

                case 4:
                {
                    string oldPin = readHiddenPin("Current PIN");
                    string newPin = readHiddenPin("New PIN (4 digits)");
                    string confirmPin = readHiddenPin("Confirm New PIN");
                    if (newPin != confirmPin)
                    {
                        cout << "\n  PINs do not match." << endl;
                    }
                    else if (acc->changePin(oldPin, newPin))
                    {
                        manager.saveState();
                        cout << "\n  PIN changed successfully." << endl;
                    }
                    else
                    {
                        cout << "\n  Incorrect current PIN or invalid new PIN." << endl;
                    }
                    break;
                }

                case 5:
                    acc->showTransactionHistory();
                    break;

                case 0:
                    loggedIn = false;
                    cout << "\n  Logged out. Thank you, " << acc->getHolderName() << "!" << endl;
                    break;

                default:
                    cout << "\n  Invalid option." << endl;
            }
        }
    }

public:
    void run()
    {
        cout << "\n  ╔══════════════════════════════════════════╗" << endl;
        cout << "  ║         ATM Management System            ║" << endl;
        cout << "  ║         Secure Banking Interface         ║" << endl;
        cout << "  ╚══════════════════════════════════════════╝" << endl;

        bool running = true;
        while (running)
        {
            if (manager.isLocked())
            {
                cout << "\n  Your account access is locked due to multiple failed attempts." << endl;
                cout << "  Please contact your bank." << endl;
                break;
            }

            cout << "\n  Enter Account Number (or 0 to exit): ";
            string accNo;
            cin >> accNo;

            if (accNo == "0") { cout << "\n  Thank you for using our ATM." << endl; break; }

            string pin = readHiddenPin("Enter PIN");

            Account* acc = manager.authenticate(accNo, pin);

            if (acc == nullptr)
            {
                if (manager.isLocked())
                    cout << "\n  Too many failed attempts. Access locked." << endl;
                else
                    cout << "\n  Invalid account number or PIN. "
                         << manager.getRemainingAttempts() << " attempt(s) remaining." << endl;
            }
            else
            {
                cout << "\n  Welcome, " << acc->getHolderName() << "!" << endl;
                showAccountMenu(acc);
            }
        }
    }
};

int main()
{
    ATMInterface atm;
    atm.run();
    return 0;
}
