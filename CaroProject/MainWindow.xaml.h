#pragma once
#include "MainWindow.g.h"

namespace winrt::CaroProject::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void InitBoard();

        // Da doi thanh fire_and_forget de ho tro hien thi bang thong bao
        winrt::fire_and_forget OnCellClicked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void RestartButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        // =========================================================
        // CAC HAM PHUC VU THUAT TOAN THAM LAM (GREEDY STRATEGY)
        // =========================================================
        void HintButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        int EvaluateCell(int r, int c, int player);
        // =========================================================

        bool CheckWin(int r, int c, int player);

    private:
        static const int BOARD_SIZE = 15;
        static const int EMPTY = 0;
        static const int PLAYER_1 = 1;
        static const int PLAYER_2 = 2;

        int board[BOARD_SIZE][BOARD_SIZE];
        int currentPlayer;
        bool isGameOver;
    };
}

namespace winrt::CaroProject::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}