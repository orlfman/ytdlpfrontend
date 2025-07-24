#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMainWindow>
#include <QProcess>
#include <QDateTime>
#include <QListWidget>
#include <QRadioButton>
#include <QButtonGroup>
#include <QStatusBar>
#include <algorithm>
#include <QTableWidget>
#include <QHeaderView>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QVector>
#include <QPair>

class QLineEdit;
class QPushButton;
class QTabWidget;
class QComboBox;
class QCheckBox;
class QSlider;
class QLabel;
class QProgressBar;
class QTextEdit;

class BookmarkDialog : public QDialog {
    Q_OBJECT
public:
    BookmarkDialog(const QString& url, const QString& name = QString(), const QString& outputDir = QString(),
                   const QString& filenameFormat = QString(), const QString& outputDirFormat = QString(),
                   bool useSubdir = false, const QString& subdirName = QString(),
                   const QString& listLimit = "All", const QString& selectedFormatCode = QString(), QWidget* parent = nullptr);
    QString getName() const;
    QString getOutputDir() const;
    QString getFilenameFormat() const;
    QString getOutputDirFormat() const;
    bool getUseSubdir() const;
    QString getSubdirName() const;
    QString getListLimit() const;
    QString getSelectedFormatCode() const;

private:
    QLineEdit* nameEdit;
    QLineEdit* outputDirEdit;
    QPushButton* browseButton;
    QLineEdit* filenameFormatEdit;
    QLineEdit* outputDirFormatEdit;
    QCheckBox* useSubdirCheck;
    QLineEdit* subdirNameEdit;
    QComboBox* listLimitCombo;
    QLineEdit* selectedFormatCodeEdit;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    Q_SIGNAL void downloadFinished();
    QString generatePlaylistItems(const QList<int>& indices);

private slots:
    void onSelectFolderClicked();
    void onDownloadClicked();
    void onCancelClicked();
    void onCancelCleanup();
    void onProcessOutput();
    void onUseCookiesFileToggled(bool checked);
    void onExtractCookiesFromBrowserToggled(bool checked);
    void onBrowseCookiesFileClicked();
    void updateCommandPreview();
    void onSaveConfigClicked();
    void onListFormatsClicked();
    void startDownload();
    void onRegexComboBoxChanged(int index);
    void onListPlaylistClicked();
    void onListChannelClicked();
    void onChannelSearchTextChanged(const QString &text);
    void onPlaylistSearchTextChanged(const QString &text);
    void updateUseSelectedChannelCheck();
    void onBookmarkClicked();
    void onBookmarkTableSelectionChanged();
    void onRemoveBookmarkClicked();
    void onEditBookmarkClicked();
    void onUrlFetchFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    static const int OriginalIndexRole = Qt::UserRole + 1;
    void setupUi();
    QStringList buildCommand();
    QStringList getSelectedChannelVideoUrls(const QString& channelUrl, const QString& endpoint, const QList<int>& indices);

    enum class DownloadState { Idle, Downloading, PostProcessing };
    DownloadState downloadState = DownloadState::Idle;
    void initializeDatabase();
    void loadBookmarks();

    // Stuff
    QLineEdit *urlTextBox;
    QLineEdit *outputDirTextBox;
    QPushButton *selectFolderButton;
    QPushButton *downloadButton;
    QPushButton *cancelButton;
    QProgressBar *progressBar;
    QTextEdit *consoleTextEdit;
    QLineEdit *usernameTextBox;
    QLineEdit *passwordTextBox;
    // Cookies File
    QCheckBox *useCookiesFileCheck;
    QLineEdit *cookiesFileTextBox;
    QPushButton *browseCookiesFileButton;

    // Cookies from Browser
    QCheckBox *extractCookiesFromBrowserCheck;
    QComboBox *browserComboBox;
    QLineEdit *browserProfileTextBox;
    QCheckBox *enableAuthCheck;
    QLineEdit *commandPreviewTextBox;

    // Filename Formatting Tab
    QComboBox *formatComboBox;
    QLineEdit *customFormatTextBox;
    QComboBox *extensionComboBox;
    QCheckBox *trimFilenamesCheck;
    QSlider *trimLengthSlider;
    QLabel *trimLengthDisplay;
    QCheckBox *autoNumberCheck;
    QCheckBox *restrictFilenamesCheck;
    QCheckBox *replaceSpacesCheck;
    QCheckBox *allowUnsafeExtCheck;
    QCheckBox *allowOverwritesCheck;

    // Output Directory Formatting
    QLabel *outputDirFormatLabel;
    QComboBox *outputDirFormatComboBox;
    QLineEdit *customOutputDirFormatTextBox;
    QCheckBox *useOutputSubdirCheck;
    QLineEdit *outputSubdirTextBox;

    // General Tab
    QCheckBox *disableConfigCheck;
    QCheckBox *continueOnErrorsCheck;
    QCheckBox *forceGenericExtractorCheck;
    QCheckBox *legacyServerConnectCheck;
    QCheckBox *ignoreSslCertificateCheck;
    QCheckBox *embedThumbnailCheck;
    QCheckBox *addMetadataCheck;
    QCheckBox *embedInfoJsonCheck;
    QCheckBox *embedChaptersCheck;
    QCheckBox *sleepIntervalCheck;
    QSlider *sleepIntervalSlider;
    QLabel *sleepIntervalDisplay;
    QCheckBox *waitForStreamCheck;
    QSlider *waitForStreamSlider;
    QLabel *waitForStreamDisplay;
    QCheckBox *impersonateCheck;
    QComboBox *impersonateComboBox;
    QCheckBox *audioOnlyCheck;
    QComboBox *audioFormatCombo;

    // SponsorBlock Tab
    QCheckBox *enableSponsorBlockCheck;
    QCheckBox *sponsorCheck;
    QCheckBox *selfPromoCheck;
    QCheckBox *interactionCheck;
    QCheckBox *fillerCheck;
    QCheckBox *outroCheck;
    QCheckBox *introCheck;
    QCheckBox *previewCheck;
    QCheckBox *musicOfftopicCheck;

    // Subtitles Tab
    QCheckBox *downloadSubsCheck;
    QRadioButton *allSubsRadio;
    QRadioButton *specificSubsRadio;
    QListWidget *subsLangList;
    QCheckBox *embedSubsCheck;

    // Playlist Tab
    QPushButton *listPlaylistButton;
    QListWidget *playlistListWidget;
    QString playlistOutput;
    QCheckBox *useSelectedItemsCheck;
    QLineEdit *playlistSearchTextBox;
    QStringList originalPlaylistTitles;

    // Channel Browser Tab
    QPushButton *listChannelButton;
    QListWidget *channelListWidget;
    QString channelOutput;
    QCheckBox *useSelectedChannelItemsCheck;
    QComboBox *channelContentComboBox;
    QLineEdit *channelSearchTextBox;
    QComboBox *listLimitComboBox;
    QTableWidget *bookmarksTable;
    QCheckBox *showUploadDatesCheck;
    QLabel *videoCountLabel;

    // List Formats Tab
    QPushButton *listFormatsButton;
    QTextEdit *formatsTextEdit;
    QLineEdit *selectedFormatCodeTextBox;
    QCheckBox *useQuickFormatsCheck;
    QComboBox *quickFormatsComboBox;

    // More Stuff
    QProcess *process;
    QProcess *urlFetchProcess;
    QStringList filesToDelete;
    QDateTime downloadStartTime;
    QVector<QPair<QString, QString>> originalChannelData;
    QString channelUrl;
    QString endpoint;
    QList<int> indices;
    int fetchAttempt;
    int maxRetries;

    QCheckBox *useAria2cCheck;
    QComboBox *aria2cOptionsCombo;

    QCheckBox *downloadAdditionalUrlsCheck;
    QLineEdit *additionalUrlsRegexTextBox;
    QComboBox *videoUrlsRegexComboBox;
    QComboBox *playlistUrlsRegexComboBox;
    QComboBox *channelUrlsRegexComboBox;
    QComboBox *mobileUrlsRegexComboBox;
    QComboBox *downloadSectionsComboBox;
    QLineEdit *downloadSectionsTextBox;
    QLabel *regexSourceLabel;

    QStatusBar *statusBar;
};

#endif // MAINWINDOW_H
