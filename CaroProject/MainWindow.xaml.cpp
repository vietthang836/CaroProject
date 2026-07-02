#include "pch.h"
#include "MainWindow.xaml.h"
#include <winrt/Microsoft.UI.Text.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.System.h>

#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::Networking::Sockets;
using namespace Windows::Storage::Streams;
using namespace Windows::Networking;

namespace winrt::CaroProject::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

        for (int i = 0; i < BOARD_SIZE; ++i)
        {
            RowDefinition rowDef;
            rowDef.Height(GridLengthHelper::FromPixels(40));
            BoardGrid().RowDefinitions().Append(rowDef);

            ColumnDefinition colDef;
            colDef.Width(GridLengthHelper::FromPixels(40));
            BoardGrid().ColumnDefinitions().Append(colDef);
        }
        SetupTimer();
        // Xoa InitBoard() o day de khong chay timer khi dang o Menu
    }

    // =====================================================================
    // MAN HINH: CHUYEN TRANG THAI GIAO DIEN
    // =====================================================================
    void MainWindow::LocalPlay_Click(IInspectable const&, RoutedEventArgs const&)
    {
        isNetworkMode = false;

        MenuScreen().Visibility(Visibility::Collapsed);
        ResultScreen().Visibility(Visibility::Collapsed);
        GameScreen().Visibility(Visibility::Visible);

        StatusText().Text(L"Trang thai: Choi 2 nguoi tren cung may.");
        InitBoard();
    }

    void MainWindow::BackToMenu_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (tcpListener != nullptr) { tcpListener.Close(); tcpListener = nullptr; }
        if (networkSocket != nullptr) { networkSocket.Close(); networkSocket = nullptr; }

        ResultScreen().Visibility(Visibility::Collapsed);
        GameScreen().Visibility(Visibility::Collapsed);
        MenuScreen().Visibility(Visibility::Visible);

        MenuStatusText().Text(L"");
    }

    void MainWindow::ShowWinner(winrt::hstring const& message, bool isWin)
    {
        turnTimer.Stop();

        DispatcherQueue().TryEnqueue([this, message, isWin]() {
            GameScreen().Visibility(Visibility::Collapsed);
            MenuScreen().Visibility(Visibility::Collapsed);
            ResultScreen().Visibility(Visibility::Visible);

            WinnerText().Text(message);
            WinnerText().Foreground(SolidColorBrush(isWin ? Microsoft::UI::Colors::DarkGreen() : Microsoft::UI::Colors::DarkRed()));
            });
    }

    void MainWindow::SurrenderButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (isNetworkMode) {
            SendNetworkMessageAsync(L"TIMEOUT|");
        }
        ShowWinner(L"BAN DA DAU HANG!", false);
    }

    // =====================================================================
    // HOST GAME & JOIN GAME
    // =====================================================================
    winrt::fire_and_forget MainWindow::HostGame_Click(IInspectable const&, RoutedEventArgs const&)
    {
        try {
            if (tcpListener != nullptr) { tcpListener.Close(); tcpListener = nullptr; }
            tcpListener = StreamSocketListener();

            tcpListener.ConnectionReceived([this](StreamSocketListener const&, StreamSocketListenerConnectionReceivedEventArgs const& args)
                {
                    networkSocket = args.Socket();
                    socketWriter = DataWriter(networkSocket.OutputStream());
                    isNetworkMode = true;
                    networkRole = PLAYER_1;

                    DispatcherQueue().TryEnqueue([this]() {
                        MenuScreen().Visibility(Visibility::Collapsed);
                        GameScreen().Visibility(Visibility::Visible);
                        StatusText().Text(L"Trang thai: Da ket noi! Ban la Host (X).");
                        InitBoard();
                        });

                    ListenForDataAsync(networkSocket);
                });

            co_await tcpListener.BindServiceNameAsync(L"9000");
            MenuStatusText().Text(L"Trang thai: Dang cho doi thu (Cong 9000)...");
            MenuStatusText().Foreground(SolidColorBrush(Microsoft::UI::Colors::Green()));
        }
        catch (winrt::hresult_error const& ex) {
            MenuStatusText().Text(L"Loi tao phong: " + ex.message());
            MenuStatusText().Foreground(SolidColorBrush(Microsoft::UI::Colors::Red()));
        }
    }

    winrt::fire_and_forget MainWindow::JoinGame_Click(IInspectable const&, RoutedEventArgs const&)
    {
        try {
            winrt::hstring ipAddress = IpTextBox().Text();
            if (ipAddress.empty()) {
                MenuStatusText().Text(L"Vui long nhap IP Server!");
                MenuStatusText().Foreground(SolidColorBrush(Microsoft::UI::Colors::Red()));
                co_return;
            }

            if (networkSocket != nullptr) { networkSocket.Close(); networkSocket = nullptr; }
            MenuStatusText().Text(L"Trang thai: Dang ket noi...");
            MenuStatusText().Foreground(SolidColorBrush(Microsoft::UI::Colors::Orange()));

            networkSocket = StreamSocket();
            HostName hostName{ ipAddress };
            co_await networkSocket.ConnectAsync(hostName, L"9000");

            socketWriter = DataWriter(networkSocket.OutputStream());
            isNetworkMode = true;
            networkRole = PLAYER_2;

            DispatcherQueue().TryEnqueue([this]() {
                MenuScreen().Visibility(Visibility::Collapsed);
                GameScreen().Visibility(Visibility::Visible);
                StatusText().Text(L"Trang thai: Da ket noi! Ban la Client (O).");
                InitBoard();
                });

            ListenForDataAsync(networkSocket);
        }
        catch (winrt::hresult_error const& ex) {
            MenuStatusText().Text(L"Loi ket noi: Khong tim thay Server.");
            MenuStatusText().Foreground(SolidColorBrush(Microsoft::UI::Colors::Red()));
        }
    }

    // =====================================================================
    // TRUYEN NHAN MANG DA NANG
    // =====================================================================
    winrt::fire_and_forget MainWindow::SendNetworkMessageAsync(winrt::hstring const& message)
    {
        if (socketWriter == nullptr) co_return;
        try {
            socketWriter.WriteUInt32(message.size());
            socketWriter.WriteString(message);
            co_await socketWriter.StoreAsync();
        }
        catch (...) {}
    }

    winrt::fire_and_forget MainWindow::ListenForDataAsync(StreamSocket socket)
    {
        DataReader reader(socket.InputStream());
        reader.InputStreamOptions(InputStreamOptions::Partial);

        try {
            while (true) {
                unsigned int headerBytes = co_await reader.LoadAsync(sizeof(uint32_t));
                if (headerBytes != sizeof(uint32_t)) break;
                uint32_t payloadLength = reader.ReadUInt32();

                unsigned int payloadBytes = co_await reader.LoadAsync(payloadLength);
                if (payloadBytes != payloadLength) break;
                winrt::hstring rawMsg = reader.ReadString(payloadLength);
                std::wstring msgStr(rawMsg.c_str());

                DispatcherQueue().TryEnqueue([this, msgStr]() {
                    if (msgStr.rfind(L"MOVE|", 0) == 0) {
                        std::wstring coord = msgStr.substr(5);
                        size_t comma = coord.find(L',');
                        if (comma != std::wstring::npos) {
                            int r = std::stoi(coord.substr(0, comma));
                            int c = std::stoi(coord.substr(comma + 1));
                            ApplyMoveToMatrixAndUI(r, c);
                        }
                    }
                    else if (msgStr.rfind(L"CHAT|", 0) == 0) {
                        winrt::hstring chatMsg(msgStr.substr(5));
                        AppendChatMessage(L"Doi thu", chatMsg, false);
                    }
                    else if (msgStr.rfind(L"TIMEOUT|", 0) == 0) {
                        ShowWinner(L"DOI THU DA DAU HANG (HOAC HET GIO)! (THANG)", true);
                    }
                    });
            }
        }
        catch (...) {}
        DispatcherQueue().TryEnqueue([this]() { StatusText().Text(L"Trang thai: Mat ket noi mang."); });
    }

    // =====================================================================
    // LOGIC HEN GIO (TIMER)
    // =====================================================================
    void MainWindow::SetupTimer()
    {
        turnTimer = DispatcherTimer();
        turnTimer.Interval(std::chrono::seconds(1));
        turnTimer.Tick({ this, &MainWindow::OnTimerTick });
    }

    void MainWindow::ResetTimer()
    {
        timeLeft = TURN_TIME_LIMIT;
        TimerText().Text(L"Thoi gian: " + to_hstring(timeLeft) + L"s");
        turnTimer.Start();
    }

    void MainWindow::OnTimerTick(IInspectable const&, IInspectable const&)
    {
        if (isGameOver) { turnTimer.Stop(); return; }

        timeLeft--;
        TimerText().Text(L"Thoi gian: " + to_hstring(timeLeft) + L"s");

        if (timeLeft <= 0) { EndGameTimeout(); }
    }

    void MainWindow::EndGameTimeout()
    {
        isGameOver = true;
        turnTimer.Stop();

        if (isNetworkMode && currentPlayer == networkRole) {
            SendNetworkMessageAsync(L"TIMEOUT|");
            ShowWinner(L"BAN DA HET GIO! (THUA)", false);
        }
        else if (isNetworkMode && currentPlayer != networkRole) {
            ShowWinner(L"DOI THU DA HET GIO! (THANG)", true);
        }
        else {
            winrt::hstring loser = (currentPlayer == PLAYER_1) ? L"NGUOI CHOI 1" : L"NGUOI CHOI 2";
            ShowWinner(loser + L" DA HET GIO!", false);
        }
    }

    void MainWindow::InitBoard()
    {
        isGameOver = false;
        currentPlayer = PLAYER_1;

        if (isNetworkMode) {
            TurnText().Text(networkRole == PLAYER_1 ? L"Luot cua ban (X)" : L"Doi doi thu di... (X di truoc)");
        }
        else {
            TurnText().Text(L"Luot di: Nguoi choi 1 (X)");
        }

        BoardGrid().Children().Clear();
        ChatPanel().Children().Clear();

        for (int row = 0; row < BOARD_SIZE; ++row) {
            for (int col = 0; col < BOARD_SIZE; ++col) {
                board[row][col] = EMPTY;
                Button btn;
                btn.Width(40); btn.Height(40); btn.FontSize(18);
                btn.FontWeight(Microsoft::UI::Text::FontWeights::Bold());
                btn.Content(box_value(L""));
                btn.Background(SolidColorBrush(Microsoft::UI::Colors::White()));
                btn.Tag(box_value(to_hstring(row) + L"," + to_hstring(col)));
                btn.Click({ this, &MainWindow::OnCellClicked });

                Grid::SetRow(btn, row);
                Grid::SetColumn(btn, col);
                BoardGrid().Children().Append(btn);
            }
        }
        ResetTimer();
    }

    // =====================================================================
    // LOGIC DANH CO
    // =====================================================================
    winrt::fire_and_forget MainWindow::OnCellClicked(IInspectable const& sender, RoutedEventArgs const&)
    {
        if (isGameOver) co_return;
        if (isNetworkMode && currentPlayer != networkRole) co_return;

        Button btn = sender.as<Button>();
        winrt::hstring tagStr = unbox_value<winrt::hstring>(btn.Tag());
        std::wstring tagWstr(tagStr.c_str());
        size_t commaPos = tagWstr.find(L',');
        int r = std::stoi(tagWstr.substr(0, commaPos));
        int c = std::stoi(tagWstr.substr(commaPos + 1));

        if (board[r][c] != EMPTY) co_return;

        if (isNetworkMode) { SendNetworkMessageAsync(L"MOVE|" + to_hstring(r) + L"," + to_hstring(c)); }
        ApplyMoveToMatrixAndUI(r, c);
    }

    void MainWindow::ApplyMoveToMatrixAndUI(int r, int c)
    {
        if (isGameOver || board[r][c] != EMPTY) return;
        board[r][c] = currentPlayer;

        for (auto const& child : BoardGrid().Children()) {
            Button btn = child.as<Button>();
            if (unbox_value<winrt::hstring>(btn.Tag()) == (to_hstring(r) + L"," + to_hstring(c))) {
                btn.Content(box_value(currentPlayer == PLAYER_1 ? L"X" : L"O"));
                btn.Foreground(SolidColorBrush(currentPlayer == PLAYER_1 ? Microsoft::UI::Colors::Red() : Microsoft::UI::Colors::Blue()));
                break;
            }
        }

        if (CheckWin(r, c, currentPlayer)) {
            isGameOver = true;
            if (isNetworkMode) {
                if (currentPlayer == networkRole) ShowWinner(L"CHUC MUNG! BAN DA THANG!", true);
                else ShowWinner(L"RAT TIEC! BAN DA THUA!", false);
            }
            else {
                winrt::hstring winner = (currentPlayer == PLAYER_1) ? L"NGUOI CHOI 1 (X) THANG!" : L"NGUOI CHOI 2 (O) THANG!";
                ShowWinner(winner, true);
            }
            return;
        }

        currentPlayer = (currentPlayer == PLAYER_1) ? PLAYER_2 : PLAYER_1;
        if (isNetworkMode) {
            TurnText().Text(currentPlayer == networkRole ? L"Luot cua ban!" : L"Doi doi thu di...");
        }
        else {
            TurnText().Text(currentPlayer == PLAYER_1 ? L"Luot di: Nguoi choi 1 (X)" : L"Luot di: Nguoi choi 2 (O)");
        }
        ResetTimer();
    }

    // =====================================================================
    // CHECK WIN VA GOI Y
    // =====================================================================
    bool MainWindow::CheckWin(int r, int c, int player)
    {
        int dirX[] = { 1, 0, 1, 1 };
        int dirY[] = { 0, 1, 1, -1 };

        for (int i = 0; i < 4; i++) {
            int count = 1;
            for (int step = 1; step < 5; step++) {
                int nr = r + step * dirY[i];
                int nc = c + step * dirX[i];
                if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE || board[nr][nc] != player) break;
                count++;
            }
            for (int step = 1; step < 5; step++) {
                int nr = r - step * dirY[i];
                int nc = c - step * dirX[i];
                if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE || board[nr][nc] != player) break;
                count++;
            }
            if (count >= 5) return true;
        }
        return false;
    }

        // =====================================================================
        // THUAT TOAN THAM LAM (Greedy)
        // =====================================================================
        // Ham nay tra ve gia tri "tham lam" nhat cua o co (diem cao nhat)
    int MainWindow::EvaluateCell(int r, int c, int player)
    {
        int score = 0;
        int dirX[] = { 1, 0, 1, 1 };
        int dirY[] = { 0, 1, 1, -1 };
        int opponent = (player == PLAYER_1) ? PLAYER_2 : PLAYER_1;

        // THAM LAM: Tinh toan loi ich ngay truoc mat
        for (int i = 0; i < 4; i++) {
            int myCount = 0, oppCount = 0;

            // Dem so quan lien tiep theo cac huong
            // CACH TINH DIEM THAM LAM:
            // Luon uu tien hanh dong co diem so cao nhat tai thoi diem hien tai
            if (myCount >= 4) score += 10000;         // Tham: Can 5 quan (Thang)
            else if (oppCount >= 4) score += 5000;    // Tham: Can chan doi thu (Phong thu)
            else if (myCount == 3) score += 1000;     // Tham: Tao the 3 (Tan cong)
            else if (oppCount == 3) score += 500;     // Tham: Chan the 3
            else score += myCount * 10 + oppCount * 5;
        }
        // Tra ve diem cao nhat tich luy duoc
        return score;
    }

        // =====================================================================
        // THUAT TOAN VET CAN (Brute Force)
        // =====================================================================
        // Ham nay vet can (duyet qua) tat ca cac o trong de tim lua chon tot nhat
    void MainWindow::HintButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (isGameOver) return;
        int bestScore = -1, bestR = -1, bestC = -1;

        // VET CAN: Duyet qua 225 o cua ban co (15x15)
        for (int r = 0; r < BOARD_SIZE; r++) {
            for (int c = 0; c < BOARD_SIZE; c++) {
                // Chi xet nhung o con trong
                if (board[r][c] == EMPTY) {
                    // Tinh diem cho tung o (Ap dung logic tham lam o day)
                    int score = EvaluateCell(r, c, currentPlayer);

                    // Cap nhat neu tim thay o co diem cao hon
                    if (score > bestScore) {
                        bestScore = score; bestR = r; bestC = c;
                    }
                }
            }
        }
    }

    // =====================================================================
    // LOGIC CHAT
    // =====================================================================
    void MainWindow::AppendChatMessage(winrt::hstring const& senderName, winrt::hstring const& message, bool isMe)
    {
        TextBlock textBlock;
        textBlock.Text(senderName + L": " + message);
        textBlock.TextWrapping(TextWrapping::Wrap);

        if (isMe) {
            textBlock.Foreground(SolidColorBrush(Microsoft::UI::Colors::DarkGreen()));
            textBlock.HorizontalAlignment(HorizontalAlignment::Right);
        }
        else {
            textBlock.Foreground(SolidColorBrush(Microsoft::UI::Colors::DarkBlue()));
            textBlock.HorizontalAlignment(HorizontalAlignment::Left);
        }

        ChatPanel().Children().Append(textBlock);
        ChatScroll().UpdateLayout();
        ChatScroll().ChangeView(nullptr, ChatScroll().ScrollableHeight(), nullptr);
    }

    void MainWindow::SendChat_Click(IInspectable const&, RoutedEventArgs const&)
    {
        winrt::hstring msg = ChatInputTextBox().Text();
        if (msg.empty()) return;

        AppendChatMessage(L"Toi", msg, true);
        if (isNetworkMode) SendNetworkMessageAsync(L"CHAT|" + msg);
        ChatInputTextBox().Text(L"");
    }

    void MainWindow::ChatInputTextBox_KeyDown(IInspectable const&, Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args)
    {
        if (args.Key() == winrt::Windows::System::VirtualKey::Enter) {
            SendChat_Click(nullptr, nullptr);
        }
    }
}