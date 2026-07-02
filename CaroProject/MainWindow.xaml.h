#pragma once
#include "MainWindow.g.h"
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Networking.h>
#include <winrt/Microsoft.UI.Xaml.h>

namespace winrt::CaroProject::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void InitBoard();
        winrt::fire_and_forget OnCellClicked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void HintButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

        int EvaluateCell(int r, int c, int player);
        bool CheckWin(int r, int c, int player);

        // -- CAC HAM MAN HINH MENU --
        void LocalPlay_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void BackToMenu_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void SurrenderButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void ShowWinner(winrt::hstring const& message, bool isWin);

        // -- CAC HAM MANG --
        winrt::fire_and_forget HostGame_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget JoinGame_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        winrt::fire_and_forget ListenForDataAsync(winrt::Windows::Networking::Sockets::StreamSocket socket);
        winrt::fire_and_forget SendNetworkMessageAsync(winrt::hstring const& message);
        void ApplyMoveToMatrixAndUI(int r, int c);

        // -- TIMER --
        void SetupTimer();
        void OnTimerTick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);
        void ResetTimer();
        void EndGameTimeout();

        // -- CHAT --
        void AppendChatMessage(winrt::hstring const& senderName, winrt::hstring const& message, bool isMe);
        void SendChat_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void ChatInputTextBox_KeyDown(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);

    private:
        static const int BOARD_SIZE = 15;
        static const int EMPTY = 0;
        static const int PLAYER_1 = 1;
        static const int PLAYER_2 = 2;
        static const int TURN_TIME_LIMIT = 30;

        int board[BOARD_SIZE][BOARD_SIZE];
        int currentPlayer;
        bool isGameOver;

        bool isNetworkMode = false;
        int networkRole = 0;

        winrt::Windows::Networking::Sockets::StreamSocketListener tcpListener{ nullptr };
        winrt::Windows::Networking::Sockets::StreamSocket networkSocket{ nullptr };
        winrt::Windows::Storage::Streams::DataWriter socketWriter{ nullptr };
        winrt::Microsoft::UI::Xaml::DispatcherTimer turnTimer{ nullptr };
        int timeLeft;
    };
}
namespace winrt::CaroProject::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}