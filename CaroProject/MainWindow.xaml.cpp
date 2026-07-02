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
        InitBoard();
    }

    // =====================================================================
    // LOGIC HẸN GIỜ (TIMER)
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
        TimerText().Text(L"⏳ Thời gian: " + to_hstring(timeLeft) + L"s");
        turnTimer.Start();
    }

    void MainWindow::OnTimerTick(IInspectable const&, IInspectable const&)
    {
        if (isGameOver) {
            turnTimer.Stop();
            return;
        }

        timeLeft--;
        TimerText().Text(L"⏳ Thời gian: " + to_hstring(timeLeft) + L"s");

        // Nếu hết giờ
        if (timeLeft <= 0) {
            turnTimer.Stop();
            EndGameTimeout();
        }
    }

    void MainWindow::EndGameTimeout()
    {
        isGameOver = true;
        winrt::hstring loser = (currentPlayer == PLAYER_1) ? L"Người chơi 1 (X)" : L"Người chơi 2 (O)";
        TurnText().Text(loser + L" đã hết giờ!");
        StatusText().Text(L"Trận đấu kết thúc do hết thời gian.");

        // Nếu đang chơi mạng và chính LÀ MÌNH hết giờ -> Gửi thông báo đầu hàng cho đối thủ
        if (isNetworkMode && currentPlayer == networkRole) {
            SendNetworkMessageAsync(L"TIMEOUT|");
        }
    }

    void MainWindow::InitBoard()
    {
        isGameOver = false;
        currentPlayer = PLAYER_1;

        if (isNetworkMode) {
            TurnText().Text(networkRole == PLAYER_1 ? L"Lượt của bạn (X)" : L"Đợi đối thủ đi... (X đi trước)");
        }
        else {
            TurnText().Text(L"Luot di: Nguoi choi 1 (X)");
        }

        BoardGrid().Children().Clear();
        ChatPanel().Children().Clear(); // Xóa chat cũ khi chơi lại

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
        ResetTimer(); // Bắt đầu đếm giờ
    }

    // =====================================================================
    // GIAO THỨC MẠNG ĐA NĂNG (GỬI CHAT & TỌA ĐỘ)
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
                    // 1. Phân tích gói tin ĐÁNH CỜ
                    if (msgStr.rfind(L"MOVE|", 0) == 0) {
                        std::wstring coord = msgStr.substr(5);
                        size_t comma = coord.find(L',');
                        if (comma != std::wstring::npos) {
                            int r = std::stoi(coord.substr(0, comma));
                            int c = std::stoi(coord.substr(comma + 1));
                            ApplyMoveToMatrixAndUI(r, c);
                        }
                    }
                    // 2. Phân tích gói tin CHAT
                    else if (msgStr.rfind(L"CHAT|", 0) == 0) {
                        winrt::hstring chatMsg(msgStr.substr(5));
                        AppendChatMessage(L"Đối thủ", chatMsg, false);
                    }
                    // 3. Phân tích gói tin HẾT GIỜ
                    else if (msgStr.rfind(L"TIMEOUT|", 0) == 0) {
                        EndGameTimeout();
                    }
                    });
            }
        }
        catch (...) {}
        DispatcherQueue().TryEnqueue([this]() { StatusText().Text(L"Trạng thái: Mất kết nối mạng."); });
    }

    // =====================================================================
    // LOGIC ĐÁNH CỜ VÀ CHAT UI
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

        if (isNetworkMode) {
            // Đóng gói với tiền tố MOVE|
            SendNetworkMessageAsync(L"MOVE|" + to_hstring(r) + L"," + to_hstring(c));
        }

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
            turnTimer.Stop();
            TurnText().Text(currentPlayer == PLAYER_1 ? L"Người chơi 1 (X) Thắng!" : L"Người chơi 2 (O) Thắng!");
            return;
        }

        currentPlayer = (currentPlayer == PLAYER_1) ? PLAYER_2 : PLAYER_1;

        if (isNetworkMode) {
            TurnText().Text(currentPlayer == networkRole ? L"Lượt của bạn!" : L"Đợi đối thủ đi...");
        }
        else {
            TurnText().Text(currentPlayer == PLAYER_1 ? L"Luot di: Nguoi choi 1 (X)" : L"Luot di: Nguoi choi 2 (O)");
        }

        ResetTimer(); // Chuyển lượt thì Reset lại đồng hồ
    }

    // =====================================================================
    // LOGIC XỬ LÝ KHUNG CHAT
    // =====================================================================
    void MainWindow::AppendChatMessage(winrt::hstring const& senderName, winrt::hstring const& message, bool isMe)
    {
        TextBlock textBlock;
        textBlock.Text(senderName + L": " + message);
        textBlock.TextWrapping(TextWrapping::Wrap);

        // Màu tin nhắn phân biệt mình và địch
        if (isMe) {
            textBlock.Foreground(SolidColorBrush(Microsoft::UI::Colors::DarkGreen()));
            textBlock.HorizontalAlignment(HorizontalAlignment::Right);
        }
        else {
            textBlock.Foreground(SolidColorBrush(Microsoft::UI::Colors::DarkBlue()));
            textBlock.HorizontalAlignment(HorizontalAlignment::Left);
        }

        ChatPanel().Children().Append(textBlock);

        // Tự động cuộn xuống dòng mới nhất
        ChatScroll().UpdateLayout();
        ChatScroll().ChangeView(nullptr, ChatScroll().ScrollableHeight(), nullptr);
    }

    void MainWindow::SendChat_Click(IInspectable const&, RoutedEventArgs const&)
    {
        winrt::hstring msg = ChatInputTextBox().Text();
        if (msg.empty()) return;

        AppendChatMessage(L"Tôi", msg, true);

        if (isNetworkMode) {
            // Đóng gói với tiền tố CHAT|
            SendNetworkMessageAsync(L"CHAT|" + msg);
        }

        ChatInputTextBox().Text(L""); // Xóa ô nhập liệu sau khi gửi
    }

    void MainWindow::ChatInputTextBox_KeyDown(IInspectable const&, Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args)
    {
        // Cho phép ấn Enter để gửi Chat
        if (args.Key() == winrt::Windows::System::VirtualKey::Enter) {
            SendChat_Click(nullptr, nullptr);
        }
    }
    // =====================================================================
// CHECK WIN
// =====================================================================
    bool MainWindow::CheckWin(int r, int c, int player)
    {
        int dirX[] = { 1, 0, 1, 1 };
        int dirY[] = { 0, 1, 1, -1 };

        for (int i = 0; i < 4; i++)
        {
            int count = 1;

            for (int step = 1; step < 5; step++)
            {
                int nr = r + step * dirY[i];
                int nc = c + step * dirX[i];

                if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE)
                    break;

                if (board[nr][nc] == player)
                    count++;
                else
                    break;
            }

            for (int step = 1; step < 5; step++)
            {
                int nr = r - step * dirY[i];
                int nc = c - step * dirX[i];

                if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE)
                    break;

                if (board[nr][nc] == player)
                    count++;
                else
                    break;
            }

            if (count >= 5)
                return true;
        }

        return false;
    }

    // =====================================================================
    // GREEDY HINT
    // =====================================================================
    int MainWindow::EvaluateCell(int r, int c, int player)
    {
        int score = 0;

        int dirX[] = { 1, 0, 1, 1 };
        int dirY[] = { 0, 1, 1, -1 };

        int opponent = (player == PLAYER_1) ? PLAYER_2 : PLAYER_1;

        for (int i = 0; i < 4; i++)
        {
            int myCount = 0;
            int oppCount = 0;

            for (int step = 1; step < 5; step++)
            {
                int nr = r + step * dirY[i];
                int nc = c + step * dirX[i];

                if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE)
                    break;

                if (board[nr][nc] == player)
                    myCount++;
                else
                    break;
            }

            for (int step = 1; step < 5; step++)
            {
                int nr = r - step * dirY[i];
                int nc = c - step * dirX[i];

                if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE)
                    break;

                if (board[nr][nc] == player)
                    myCount++;
                else
                    break;
            }

            for (int step = 1; step < 5; step++)
            {
                int nr = r + step * dirY[i];
                int nc = c + step * dirX[i];

                if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE)
                    break;

                if (board[nr][nc] == opponent)
                    oppCount++;
                else
                    break;
            }

            for (int step = 1; step < 5; step++)
            {
                int nr = r - step * dirY[i];
                int nc = c - step * dirX[i];

                if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE)
                    break;

                if (board[nr][nc] == opponent)
                    oppCount++;
                else
                    break;
            }

            if (myCount >= 4)
                score += 10000;
            else if (oppCount >= 4)
                score += 5000;
            else if (myCount == 3)
                score += 1000;
            else if (oppCount == 3)
                score += 500;
            else
                score += myCount * 10 + oppCount * 5;
        }

        return score;
    }

    // =====================================================================
    // HINT BUTTON
    // =====================================================================
    void MainWindow::HintButton_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        if (isGameOver)
            return;

        int bestScore = -1;
        int bestR = -1;
        int bestC = -1;

        for (int r = 0; r < BOARD_SIZE; r++)
        {
            for (int c = 0; c < BOARD_SIZE; c++)
            {
                if (board[r][c] == EMPTY)
                {
                    int score = EvaluateCell(r, c, currentPlayer);

                    if (score > bestScore)
                    {
                        bestScore = score;
                        bestR = r;
                        bestC = c;
                    }
                }
            }
        }

        if (bestR == -1)
            return;

        for (auto const& child : BoardGrid().Children())
        {
            Button btn = child.as<Button>();

            if (unbox_value<hstring>(btn.Tag()) ==
                (to_hstring(bestR) + L"," + to_hstring(bestC)))
            {
                btn.Background(
                    SolidColorBrush(Microsoft::UI::Colors::Yellow()));

                break;
            }
        }
    }
    // =====================================================================
    // RESTART
    // =====================================================================
    void MainWindow::RestartButton_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        InitBoard();
    }

    // =====================================================================
    // HOST GAME
    // =====================================================================
    winrt::fire_and_forget MainWindow::HostGame_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        StatusText().Text(L"Host chưa được khôi phục.");
        co_return;
    }

    // =====================================================================
    // JOIN GAME
    // =====================================================================
    winrt::fire_and_forget MainWindow::JoinGame_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        StatusText().Text(L"Join chưa được khôi phục.");
        co_return;
    }
}