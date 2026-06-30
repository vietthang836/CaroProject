#include "pch.h"
#include "MainWindow.xaml.h"
#include <winrt/Microsoft.UI.Text.h> 

#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
using namespace std;
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::Networking::Sockets;
using namespace Windows::Storage::Streams;
using namespace Windows::Networking;

namespace CaroProject::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

        for (int i = 0; i < BOARD_SIZE; ++i) {
            RowDefinition rowDef;
            rowDef.Height(GridLengthHelper::FromPixels(40));
            BoardGrid().RowDefinitions().Append(rowDef);

            ColumnDefinition colDef;
            colDef.Width(GridLengthHelper::FromPixels(40));
            BoardGrid().ColumnDefinitions().Append(colDef);
        }
        InitBoard();
    }

    void MainWindow::InitBoard()
    {
        isGameOver = false;
        currentPlayer = PLAYER_1;
        TurnText().Text(L"Luot di: Nguoi choi 1 (X)");
        BoardGrid().Children().Clear();

        for (int row = 0; row < BOARD_SIZE; ++row) {
            for (int col = 0; col < BOARD_SIZE; ++col) {
                board[row][col] = EMPTY;

                Button btn;
                btn.Width(40);
                btn.Height(40);
                btn.FontSize(20);
                btn.FontWeight(Microsoft::UI::Text::FontWeights::Bold());
                btn.Content(box_value(L""));
                btn.Background(nullptr);

                hstring tagStr = to_hstring(row) + L"," + to_hstring(col);
                btn.Tag(box_value(tagStr));
                btn.Click({ this, &MainWindow::OnCellClicked });

                Grid::SetRow(btn, row);
                Grid::SetColumn(btn, col);
                BoardGrid().Children().Append(btn);
            }
        }
    }

    fire_and_forget MainWindow::OnCellClicked(IInspectable const& sender, RoutedEventArgs const& args)
    {
        if (isGameOver) co_return;

        Button clickedBtn = sender.as<Button>();
        hstring tagStr = unbox_value<hstring>(clickedBtn.Tag());

        wstring tagWstr(tagStr.c_str());
        size_t commaPos = tagWstr.find(L',');
        int row = stoi(tagWstr.substr(0, commaPos));
        int col = stoi(tagWstr.substr(commaPos + 1));

        if (board[row][col] != EMPTY) co_return;

        board[row][col] = currentPlayer;
        clickedBtn.Background(nullptr);

        if (currentPlayer == PLAYER_1) {
            clickedBtn.Content(box_value(L"X"));
            clickedBtn.Foreground(SolidColorBrush(Microsoft::UI::Colors::Red()));
        }
        else {
            clickedBtn.Content(box_value(L"O"));
            clickedBtn.Foreground(SolidColorBrush(Microsoft::UI::Colors::Blue()));
        }

        if (CheckWin(row, col, currentPlayer)) {
            isGameOver = true;
            hstring winnerMsg = (currentPlayer == PLAYER_1) ?
                L"Chuc mung! Nguoi choi 1 (X) da gianh chien thang!" :
                L"Chuc mung! Nguoi choi 2 (O) da gianh chien thang!";

            TurnText().Text((currentPlayer == PLAYER_1) ? L"Nguoi choi 1 (X) Thang!" : L"Nguoi choi 2 (O) Thang!");

            ContentDialog dialog;
            dialog.XamlRoot(BoardGrid().XamlRoot());
            dialog.Title(box_value(L"Ket thuc tran dau"));
            dialog.Content(box_value(winnerMsg));
            dialog.CloseButtonText(L"Choi lai");

            co_await dialog.ShowAsync();
            InitBoard();
            co_return;
        }

        currentPlayer = (currentPlayer == PLAYER_1) ? PLAYER_2 : PLAYER_1;
        TurnText().Text((currentPlayer == PLAYER_1) ? L"Luot di: Nguoi choi 1 (X)" : L"Luot di: Nguoi choi 2 (O)");
    }

    // =====================================================================
    // [1] THUAT TOAN THAM LAM (GREEDY STRATEGY) - Tinh diem tung o
    // Ung dung kien thuc Chuong 6: Chien luoc tham lam
    // =====================================================================
    int MainWindow::EvaluateCell(int r, int c, int player)
    {
        int score = 0;
        int dirX[] = { 1, 0, 1, 1 };
        int dirY[] = { 0, 1, 1, -1 };
        int opponent = (player == PLAYER_1) ? PLAYER_2 : PLAYER_1;

        for (int i = 0; i < 4; i++) {
            int myCount = 0, oppCount = 0;

            // Kiem tra chuoi quan cua minh (de tan cong)
            for (int step = 1; step < 5; step++) {
                int nr = r + step * dirY[i], nc = c + step * dirX[i];
                if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
                if (board[nr][nc] == player) myCount++; else break;
            }
            for (int step = 1; step < 5; step++) {
                int nr = r - step * dirY[i], nc = c - step * dirX[i];
                if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
                if (board[nr][nc] == player) myCount++; else break;
            }

            // Kiem tra chuoi quan cua dich (de phong ngu)
            for (int step = 1; step < 5; step++) {
                int nr = r + step * dirY[i], nc = c + step * dirX[i];
                if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
                if (board[nr][nc] == opponent) oppCount++; else break;
            }
            for (int step = 1; step < 5; step++) {
                int nr = r - step * dirY[i], nc = c - step * dirX[i];
                if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
                if (board[nr][nc] == opponent) oppCount++; else break;
            }

            // ---> DAY CHINH LA HAM HEURISTIC CUA THUAT TOAN THAM LAM <---
            // Thuat toan se "tham lam" uu tien chon cac loi ich cuc bo lon nhat
            if (myCount >= 4) score += 10000;       // Do uu tien 1: An 5 de thang ngay lap tuc
            else if (oppCount >= 4) score += 5000;  // Do uu tien 2: Bat buoc phai chan dich thang
            else if (myCount == 3) score += 1000;   // Do uu tien 3: Xay dung the cong cua minh thanh 4
            else if (oppCount == 3) score += 500;   // Do uu tien 4: Chan the cong 3 cua dich
            else score += (myCount * 10) + (oppCount * 5); // Cong don diem cho cac the co khac
        }
        return score;
    }

    void MainWindow::HintButton_Click(IInspectable const& sender, RoutedEventArgs const& args)
    {
        if (isGameOver) return;

        int bestScore = -1;
        int bestR = -1, bestC = -1;

        // ---> THUAT TOAN TIM KIEM TOAN BO (EXHAUSTIVE SEARCH) VA CHON LOC THAM LAM <---
        // Quet qua mang 2 chieu (ban co) de tim vi tri toi uu cuc bo phia tren
        for (int r = 0; r < BOARD_SIZE; ++r) {
            for (int c = 0; c < BOARD_SIZE; ++c) {
                if (board[r][c] == EMPTY) {
                    int score = EvaluateCell(r, c, currentPlayer);
                    if (score > bestScore) {
                        bestScore = score;
                        bestR = r;
                        bestC = c;
                    }
                }
            }
        }

        // Hien thi ket qua cua thuat toan Greedy len UI
        if (bestR != -1 && bestC != -1) {
            for (auto const& child : BoardGrid().Children()) {
                Button btn = child.as<Button>();
                winrt::hstring tagStr = unbox_value<hstring>(btn.Tag());
                wstring tagWstr(tagStr.c_str());
                size_t commaPos = tagWstr.find(L',');
                int r = stoi(tagWstr.substr(0, commaPos));
                int c = stoi(tagWstr.substr(commaPos + 1));

                if (r == bestR && c == bestC) {
                    btn.Background(SolidColorBrush(Microsoft::UI::Colors::Yellow()));
                    break;
                }
            }
        }
    }

    // =====================================================================
    // [2] CHIEN LUOC VET CAN (BRUTE-FORCE SEARCH) 
    // Ung dung kien thuc Chuong 3: De quy va chien luoc Vet can
    // =====================================================================
    bool MainWindow::CheckWin(int r, int c, int player)
    {
        int dirX[] = { 1, 0, 1, 1 };
        int dirY[] = { 0, 1, 1, -1 };

        // ---> DAY LA GIAI THUAT VET CAN <---
        // Moi khi danh 1 quan, thuat toan se quet thu cong ca 4 huong (Ngang, doc, 2 cheo)
        // De dem so quan co, chap nhan do phuc tap la O(K) voi K la so buoc quet
        for (int i = 0; i < 4; i++) {
            int count = 1;

            // Vet can chieu tien
            for (int step = 1; step < 5; step++) {
                int nr = r + step * dirY[i];
                int nc = c + step * dirX[i];
                if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
                if (board[nr][nc] == player) count++; else break;
            }

            // Vet can chieu lui
            for (int step = 1; step < 5; step++) {
                int nr = r - step * dirY[i];
                int nc = c - step * dirX[i];
                if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
                if (board[nr][nc] == player) count++; else break;
            }

            if (count >= 5) return true;
        }
        return false;
    }

    void MainWindow::RestartButton_Click(IInspectable const& sender, RoutedEventArgs const& args)
    {
        InitBoard();
    }
}