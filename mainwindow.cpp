#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QProgressBar>
#include <QTextEdit>
#include <QFileDialog>
#include <QDir>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QThread>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QFile>
#include <QMessageBox>

BookmarkDialog::BookmarkDialog(const QString& url, const QString& name, const QString& outputDir,
                               const QString& filenameFormat, const QString& outputDirFormat,
                               bool useSubdir, const QString& subdirName, const QString& listLimit,
                               const QString& selectedFormatCode, QWidget* parent)
: QDialog(parent) {
    setWindowTitle(name.isEmpty() ? "Add Bookmark" : "Edit Bookmark");
    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* urlLabel = new QLabel("URL: " + url);
    layout->addWidget(urlLabel);

    QLabel* nameLabel = new QLabel("Bookmark Name:");
    layout->addWidget(nameLabel);
    nameEdit = new QLineEdit(name);
    layout->addWidget(nameEdit);

    QLabel* outputDirLabel = new QLabel("Output Directory:");
    layout->addWidget(outputDirLabel);
    outputDirEdit = new QLineEdit(outputDir);
    layout->addWidget(outputDirEdit);
    browseButton = new QPushButton("Browse");
    connect(browseButton, &QPushButton::clicked, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Output Directory", outputDirEdit->text());
        if (!dir.isEmpty()) {
            outputDirEdit->setText(dir);
        }
    });
    layout->addWidget(browseButton);

    QLabel* filenameFormatLabel = new QLabel("Filename Format:");
    layout->addWidget(filenameFormatLabel);
    filenameFormatEdit = new QLineEdit(filenameFormat);
    layout->addWidget(filenameFormatEdit);

    QLabel* outputDirFormatLabel = new QLabel("Output Directory Format:");
    layout->addWidget(outputDirFormatLabel);
    outputDirFormatEdit = new QLineEdit(outputDirFormat);
    layout->addWidget(outputDirFormatEdit);

    useSubdirCheck = new QCheckBox("Use 'yt-dlp output' Subdirectory");
    useSubdirCheck->setChecked(useSubdir);
    layout->addWidget(useSubdirCheck);
    subdirNameEdit = new QLineEdit(subdirName);
    subdirNameEdit->setEnabled(useSubdir);
    layout->addWidget(subdirNameEdit);
    connect(useSubdirCheck, &QCheckBox::toggled, subdirNameEdit, &QLineEdit::setEnabled);

    QLabel* listLimitLabel = new QLabel("List Limit:");
    layout->addWidget(listLimitLabel);
    listLimitCombo = new QComboBox;
    listLimitCombo->addItems({"All", "1000", "500", "250", "100", "50", "25", "10", "5", "1"});
    listLimitCombo->setCurrentText(listLimit);
    layout->addWidget(listLimitCombo);

    QLabel* selectedFormatCodeLabel = new QLabel("Selected Format Code:");
    layout->addWidget(selectedFormatCodeLabel);
    selectedFormatCodeEdit = new QLineEdit(selectedFormatCode);
    layout->addWidget(selectedFormatCodeEdit);

    QHBoxLayout* buttonLayout = new QHBoxLayout;
    QPushButton* okButton = new QPushButton("OK");
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    QPushButton* cancelButton = new QPushButton("Cancel");
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);
}

QString BookmarkDialog::getName() const {
    return nameEdit->text().trimmed();
}

QString BookmarkDialog::getOutputDir() const {
    return outputDirEdit->text().trimmed();
}

QString BookmarkDialog::getFilenameFormat() const {
    return filenameFormatEdit->text().trimmed();
}

QString BookmarkDialog::getOutputDirFormat() const {
    return outputDirFormatEdit->text().trimmed();
}

bool BookmarkDialog::getUseSubdir() const {
    return useSubdirCheck->isChecked();
}

QString BookmarkDialog::getSubdirName() const {
    return subdirNameEdit->text().trimmed();
}

QString BookmarkDialog::getListLimit() const {
    return listLimitCombo->currentText();
}

QString BookmarkDialog::getSelectedFormatCode() const {
    return selectedFormatCodeEdit->text().trimmed();
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), process(nullptr), urlFetchProcess(nullptr) {
    setupUi();
    initializeDatabase();
    loadBookmarks();

    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (defaultDir.isEmpty()) {
        defaultDir = QDir::homePath();
    }
    outputDirTextBox->setText(defaultDir);
    outputSubdirTextBox->setText("yt-dlp output");

    trimLengthDisplay->setText("50");
    sleepIntervalDisplay->setText("5s");
    waitForStreamDisplay->setText("30s");

    originalChannelData.clear();
    originalPlaylistTitles.clear();
    channelOutput.clear();

    updateCommandPreview();
}

MainWindow::~MainWindow() {
    if (process) {
        if (process->state() == QProcess::Running) {
            process->kill();
            process->waitForFinished(1000);
        }
        delete process;
        process = nullptr;
    }
    if (urlFetchProcess) {
        if (urlFetchProcess->state() == QProcess::Running) {
            urlFetchProcess->kill();
            urlFetchProcess->waitForFinished(1000);
        }
        delete urlFetchProcess;
        urlFetchProcess = nullptr;
    }
    QSqlDatabase::database().close();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
}

void MainWindow::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setSpacing(6);
    mainLayout->setContentsMargins(2, 2, 2, 2);

    QHBoxLayout *urlLayout = new QHBoxLayout;
    urlLayout->setSpacing(2);
    urlLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *urlLabel = new QLabel("Video URL:");
    urlLabel->setContentsMargins(0, 0, 0, 0);
    urlLayout->addWidget(urlLabel);
    urlTextBox = new QLineEdit;
    urlTextBox->setPlaceholderText("Enter YouTube URLs separated by spaces");
    urlTextBox->setMinimumWidth(500);
    urlTextBox->setFixedHeight(20);
    urlTextBox->setContentsMargins(0, 0, 0, 0);
    connect(urlTextBox, &QLineEdit::textChanged, this, &MainWindow::updateCommandPreview);
    urlLayout->addWidget(urlTextBox);
    mainLayout->addLayout(urlLayout);

    QHBoxLayout *outputLayout = new QHBoxLayout;
    outputLayout->setSpacing(2);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *outputLabel = new QLabel("Output Directory:");
    outputLabel->setContentsMargins(0, 0, 0, 0);
    outputLayout->addWidget(outputLabel);
    outputDirTextBox = new QLineEdit;
    outputDirTextBox->setReadOnly(true);
    outputDirTextBox->setMinimumWidth(500);
    outputDirTextBox->setFixedHeight(20);
    outputDirTextBox->setContentsMargins(0, 0, 0, 0);
    outputLayout->addWidget(outputDirTextBox);
    selectFolderButton = new QPushButton("Select Folder");
    selectFolderButton->setFixedHeight(20);
    selectFolderButton->setContentsMargins(0, 0, 0, 0);
    connect(selectFolderButton, &QPushButton::clicked, this, &MainWindow::onSelectFolderClicked);
    outputLayout->addWidget(selectFolderButton);
    mainLayout->addLayout(outputLayout);

    QTabWidget *tabWidget = new QTabWidget;
    tabWidget->setStyleSheet("QTabWidget::pane { border: 1px solid #555555; margin: 0; padding: 0px; }");

    QWidget *filenameTab = new QWidget;
    QVBoxLayout *filenameLayout = new QVBoxLayout;
    filenameLayout->setSpacing(4);
    filenameLayout->setContentsMargins(5, 5, 5, 5);

    QLabel *formatLabel = new QLabel("Select Filename Format:");
    formatLabel->setContentsMargins(0, 0, 0, 0);
    filenameLayout->addWidget(formatLabel);
    formatComboBox = new QComboBox;
    formatComboBox->addItems({
        "%(uploader)s - %(title)s",
        "%(uploader)s %(title)s - %(upload_date)s",
        "%(title)s",
        "%(title)s - %(uploader)s",
        "%(upload_date)s - %(title)s",
        "%(title)s [%(id)s]",
        "%(channel)s - %(title)s",
        "%(channel)s",
        "%(playlist)s",
        "%(playlist)s - %(playlist_index)s",
        "%(playlist_index)s",
        "%(view_count)s",
        "%(like_count)s",
        "%(extractor)s",
        "%(title)s - %(duration)s",
        "%(uploader)s - %(id)s",
        "%(title)s - %(resolution)s"
    });
    formatComboBox->setCurrentIndex(2);
    formatComboBox->setFixedHeight(20);
    formatComboBox->setContentsMargins(0, 0, 0, 0);
    connect(formatComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateCommandPreview);
    filenameLayout->addWidget(formatComboBox);
    customFormatTextBox = new QLineEdit;
    customFormatTextBox->setText(formatComboBox->currentText());
    customFormatTextBox->setFixedHeight(20);
    customFormatTextBox->setContentsMargins(0, 0, 0, 0);
    connect(customFormatTextBox, &QLineEdit::textChanged, this, &MainWindow::updateCommandPreview);
    filenameLayout->addWidget(customFormatTextBox);
    connect(formatComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        customFormatTextBox->setText(formatComboBox->itemText(index));
        updateCommandPreview();
    });

    QLabel *outputDirFormatLabel = new QLabel("Select Output Directory Format:");
    outputDirFormatLabel->setContentsMargins(0, 0, 0, 0);
    filenameLayout->addWidget(outputDirFormatLabel);
    outputDirFormatComboBox = new QComboBox;
    outputDirFormatComboBox->addItems({
        "Default",
        "%(channel)s",
        "%(uploader)s",
        "%(playlist)s",
        "%(extractor)s",
        "%(upload_date)s",
        "%(channel)s/%(playlist)s",
        "%(uploader)s/%(upload_date)s",
        "%(extractor)s/%(channel)s"
    });
    outputDirFormatComboBox->setCurrentIndex(0);
    outputDirFormatComboBox->setFixedHeight(20);
    outputDirFormatComboBox->setContentsMargins(0, 0, 0, 0);
    connect(outputDirFormatComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateCommandPreview);
    filenameLayout->addWidget(outputDirFormatComboBox);
    customOutputDirFormatTextBox = new QLineEdit;
    customOutputDirFormatTextBox->setText(outputDirFormatComboBox->currentText());
    customOutputDirFormatTextBox->setFixedHeight(20);
    customOutputDirFormatTextBox->setContentsMargins(0, 0, 0, 0);
    connect(customOutputDirFormatTextBox, &QLineEdit::textChanged, this, &MainWindow::updateCommandPreview);
    filenameLayout->addWidget(customOutputDirFormatTextBox);
    connect(outputDirFormatComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        customOutputDirFormatTextBox->setText(outputDirFormatComboBox->itemText(index));
        updateCommandPreview();
    });
    connect(customOutputDirFormatTextBox, &QLineEdit::textEdited, [this](const QString &text) {
        if (text.isEmpty()) {
            outputDirFormatComboBox->setCurrentIndex(0);
        } else {
            int index = outputDirFormatComboBox->findText(text);
            if (index != -1) {
                outputDirFormatComboBox->setCurrentIndex(index);
            } else {
                outputDirFormatComboBox->setCurrentIndex(0);
            }
        }
    });

    useOutputSubdirCheck = new QCheckBox("Use 'yt-dlp output' Subdirectory");
    useOutputSubdirCheck->setChecked(true);
    useOutputSubdirCheck->setContentsMargins(0, 0, 0, 0);
    connect(useOutputSubdirCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    filenameLayout->addWidget(useOutputSubdirCheck);
    outputSubdirTextBox = new QLineEdit;
    outputSubdirTextBox->setText("yt-dlp output");
    outputSubdirTextBox->setFixedHeight(20);
    outputSubdirTextBox->setContentsMargins(0, 0, 0, 0);
    outputSubdirTextBox->setEnabled(useOutputSubdirCheck->isChecked());
    connect(useOutputSubdirCheck, &QCheckBox::toggled, outputSubdirTextBox, &QLineEdit::setEnabled);
    connect(outputSubdirTextBox, &QLineEdit::textChanged, this, &MainWindow::updateCommandPreview);
    filenameLayout->addWidget(outputSubdirTextBox);

    QLabel *extensionLabel = new QLabel("Select File Extension:");
    extensionLabel->setContentsMargins(0, 0, 0, 0);
    filenameLayout->addWidget(extensionLabel);
    extensionComboBox = new QComboBox;
    extensionComboBox->addItems({"Default", "mkv", "mp4", "webm"});
    extensionComboBox->setFixedHeight(20);
    extensionComboBox->setContentsMargins(0, 0, 0, 0);
    connect(extensionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateCommandPreview);
    filenameLayout->addWidget(extensionComboBox);

    QHBoxLayout *trimLayout = new QHBoxLayout;
    trimLayout->setSpacing(2);
    trimLayout->setContentsMargins(0, 0, 0, 0);
    trimFilenamesCheck = new QCheckBox("Trim Filenames");
    trimFilenamesCheck->setContentsMargins(0, 0, 0, 0);
    connect(trimFilenamesCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    trimLayout->addWidget(trimFilenamesCheck);
    trimLengthSlider = new QSlider(Qt::Horizontal);
    trimLengthSlider->setRange(1, 100);
    trimLengthSlider->setValue(50);
    trimLengthSlider->setFixedWidth(200);
    trimLengthSlider->setFixedHeight(16);
    trimLengthSlider->setEnabled(false);
    trimLengthSlider->setContentsMargins(0, 0, 0, 0);
    connect(trimFilenamesCheck, &QCheckBox::toggled, trimLengthSlider, &QSlider::setEnabled);
    connect(trimLengthSlider, &QSlider::valueChanged, [this](int value) {
        trimLengthDisplay->setText(QString::number(value));
        updateCommandPreview();
    });
    trimLayout->addWidget(trimLengthSlider);
    trimLengthDisplay = new QLabel("50");
    trimLengthDisplay->setFixedWidth(30);
    trimLengthDisplay->setFixedHeight(16);
    trimLayout->addWidget(trimLengthDisplay);
    trimLayout->addStretch();
    filenameLayout->addLayout(trimLayout);

    autoNumberCheck = new QCheckBox("Add Numbers to Filenames");
    autoNumberCheck->setContentsMargins(0, 0, 0, 0);
    connect(autoNumberCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    filenameLayout->addWidget(autoNumberCheck);
    restrictFilenamesCheck = new QCheckBox("Remove Special Characters");
    restrictFilenamesCheck->setContentsMargins(0, 0, 0, 0);
    connect(restrictFilenamesCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    filenameLayout->addWidget(restrictFilenamesCheck);
    replaceSpacesCheck = new QCheckBox("Replace Spaces with Underscores");
    replaceSpacesCheck->setContentsMargins(0, 0, 0, 0);
    connect(replaceSpacesCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    filenameLayout->addWidget(replaceSpacesCheck);
    allowUnsafeExtCheck = new QCheckBox("Allow Unsafe Extensions");
    allowUnsafeExtCheck->setContentsMargins(0, 0, 0, 0);
    connect(allowUnsafeExtCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    filenameLayout->addWidget(allowUnsafeExtCheck);
    allowOverwritesCheck = new QCheckBox("Allow Overwrites");
    allowOverwritesCheck->setContentsMargins(0, 0, 0, 0);
    connect(allowOverwritesCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    filenameLayout->addWidget(allowOverwritesCheck);

    filenameLayout->addStretch();
    filenameTab->setLayout(filenameLayout);
    tabWidget->addTab(filenameTab, "Formatting");

    // General Tab
    QWidget *generalTab = new QWidget;
    QVBoxLayout *generalLayout = new QVBoxLayout;
    generalLayout->setSpacing(4);
    generalLayout->setContentsMargins(5, 5, 5, 5);

    disableConfigCheck = new QCheckBox("Disable Config");
    disableConfigCheck->setChecked(true);
    disableConfigCheck->setToolTip("Stops yt-dlp from using settings from a config file");
    connect(disableConfigCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    generalLayout->addWidget(disableConfigCheck);

    continueOnErrorsCheck = new QCheckBox("Continue on Errors");
    continueOnErrorsCheck->setToolTip("Keeps downloading even if some videos fail");
    connect(continueOnErrorsCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    generalLayout->addWidget(continueOnErrorsCheck);

    forceGenericExtractorCheck = new QCheckBox("Use Generic Extractor");
    forceGenericExtractorCheck->setToolTip("Uses a simple download method if needed");
    connect(forceGenericExtractorCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    generalLayout->addWidget(forceGenericExtractorCheck);

    legacyServerConnectCheck = new QCheckBox("Use Legacy Server Connect");
    legacyServerConnectCheck->setToolTip("Uses older security settings for connections");
    connect(legacyServerConnectCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    generalLayout->addWidget(legacyServerConnectCheck);

    ignoreSslCertificateCheck = new QCheckBox("Ignore SSL Certificate");
    ignoreSslCertificateCheck->setToolTip("Skips checking site security certificates");
    connect(ignoreSslCertificateCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    generalLayout->addWidget(ignoreSslCertificateCheck);

    embedThumbnailCheck = new QCheckBox("Embed Thumbnail");
    embedThumbnailCheck->setChecked(true);
    embedThumbnailCheck->setToolTip("Adds a thumbnail into the video file");
    connect(embedThumbnailCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    generalLayout->addWidget(embedThumbnailCheck);

    addMetadataCheck = new QCheckBox("Add Metadata");
    addMetadataCheck->setChecked(true);
    addMetadataCheck->setToolTip("Adds info like author to the video");
    connect(addMetadataCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    generalLayout->addWidget(addMetadataCheck);

    embedInfoJsonCheck = new QCheckBox("Embed JSON Information");
    embedInfoJsonCheck->setChecked(true);
    embedInfoJsonCheck->setToolTip("Adds video metadata in JSON format");
    connect(embedInfoJsonCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    generalLayout->addWidget(embedInfoJsonCheck);

    embedChaptersCheck = new QCheckBox("Embed Chapters");
    embedChaptersCheck->setChecked(true);
    embedChaptersCheck->setToolTip("Adds chapter markers to the video");
    connect(embedChaptersCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    generalLayout->addWidget(embedChaptersCheck);

    QHBoxLayout *sleepLayout = new QHBoxLayout;
    sleepIntervalCheck = new QCheckBox("Wait Interval");
    sleepIntervalCheck->setToolTip("Adds a delay between downloads");
    connect(sleepIntervalCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    sleepLayout->addWidget(sleepIntervalCheck);
    sleepIntervalSlider = new QSlider(Qt::Horizontal);
    sleepIntervalSlider->setRange(1, 600);
    sleepIntervalSlider->setSingleStep(1);
    sleepIntervalSlider->setValue(5);
    sleepIntervalSlider->setFixedWidth(300);
    sleepIntervalSlider->setEnabled(false);
    connect(sleepIntervalCheck, &QCheckBox::toggled, sleepIntervalSlider, &QSlider::setEnabled);
    connect(sleepIntervalSlider, &QSlider::valueChanged, [this](int value) {
        sleepIntervalDisplay->setText(QString::number(value) + "s");
        updateCommandPreview();
    });
    sleepLayout->addWidget(sleepIntervalSlider);
    sleepIntervalDisplay = new QLabel("5s");
    sleepIntervalDisplay->setFixedWidth(60);
    sleepLayout->addWidget(sleepIntervalDisplay);
    sleepLayout->addStretch();
    generalLayout->addLayout(sleepLayout);

    QHBoxLayout *waitLayout = new QHBoxLayout;
    waitForStreamCheck = new QCheckBox("Wait For Stream");
    waitForStreamCheck->setToolTip("Waits for videos to become available");
    connect(waitForStreamCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    waitLayout->addWidget(waitForStreamCheck);
    waitForStreamSlider = new QSlider(Qt::Horizontal);
    waitForStreamSlider->setRange(1, 600);
    waitForStreamSlider->setSingleStep(1);
    waitForStreamSlider->setValue(30);
    waitForStreamSlider->setFixedWidth(300);
    waitForStreamSlider->setEnabled(false);
    connect(waitForStreamCheck, &QCheckBox::toggled, waitForStreamSlider, &QSlider::setEnabled);
    connect(waitForStreamSlider, &QSlider::valueChanged, [this](int value) {
        waitForStreamDisplay->setText(QString::number(value) + "s");
        updateCommandPreview();
    });
    waitLayout->addWidget(waitForStreamSlider);
    waitForStreamDisplay = new QLabel("30s");
    waitForStreamDisplay->setFixedWidth(60);
    waitLayout->addWidget(waitForStreamDisplay);
    waitLayout->addStretch();
    generalLayout->addLayout(waitLayout);
    // Currently semi broken on Arch Linux thanks to the way curl is packaged
    QHBoxLayout *impersonateLayout = new QHBoxLayout;
    impersonateCheck = new QCheckBox("Browser Impersonation");
    impersonateCheck->setToolTip("Pretends to be a specific browser");
    connect(impersonateCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    impersonateLayout->addWidget(impersonateCheck);
    impersonateComboBox = new QComboBox;
    impersonateComboBox->addItems({
        "chrome",
        "chrome-116:windows-10",
        "chrome-133:macos-15",
        "firefox",
        "safari",
        "edge"
    });
    impersonateComboBox->setEnabled(false);
    connect(impersonateCheck, &QCheckBox::toggled, impersonateComboBox, &QComboBox::setEnabled);
    connect(impersonateComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateCommandPreview);
    impersonateLayout->addWidget(impersonateComboBox);
    impersonateLayout->addStretch();
    generalLayout->addLayout(impersonateLayout);

    // Aria2c options
    QHBoxLayout *aria2cLayout = new QHBoxLayout;
    useAria2cCheck = new QCheckBox("Use aria2c for downloading");
    useAria2cCheck->setToolTip("Use aria2c as the external downloader for faster downloads");
    aria2cLayout->addWidget(useAria2cCheck);
    aria2cOptionsCombo = new QComboBox;
    aria2cOptionsCombo->addItems({
        "2 connections, 1M split",
        "4 connections, 2M split",
        "8 connections, 4M split",
        "16 connections, 8M split"
    });
    aria2cOptionsCombo->setEnabled(false);
    aria2cOptionsCombo->setToolTip("Select the level of parallelism for aria2c");
    aria2cLayout->addWidget(aria2cOptionsCombo);
    aria2cLayout->addStretch();
    generalLayout->addLayout(aria2cLayout);

    connect(useAria2cCheck, &QCheckBox::toggled, aria2cOptionsCombo, &QComboBox::setEnabled);
    connect(useAria2cCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    connect(aria2cOptionsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateCommandPreview);

    QHBoxLayout *audioOnlyLayout = new QHBoxLayout;
    audioOnlyCheck = new QCheckBox("Download Audio Only");
    audioOnlyCheck->setToolTip("Download only the audio track");
    connect(audioOnlyCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    audioOnlyLayout->addWidget(audioOnlyCheck);
    audioFormatCombo = new QComboBox;
    audioFormatCombo->addItems({"mp3", "aac", "m4a", "opus", "vorbis"});
    audioFormatCombo->setEnabled(false);
    connect(audioOnlyCheck, &QCheckBox::toggled, audioFormatCombo, &QComboBox::setEnabled);
    connect(audioFormatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateCommandPreview);
    audioOnlyLayout->addWidget(audioFormatCombo);
    audioOnlyLayout->addStretch();
    generalLayout->addLayout(audioOnlyLayout);

    // Download Sections
    QHBoxLayout *downloadSectionsLayout = new QHBoxLayout;
    downloadSectionsLayout->setSpacing(2);
    downloadSectionsLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *downloadSectionsLabel = new QLabel("Download Sections:");
    downloadSectionsLabel->setToolTip("Specify which parts of the video to download");
    downloadSectionsLayout->addWidget(downloadSectionsLabel);

    downloadSectionsComboBox = new QComboBox;
    downloadSectionsComboBox->addItems({
        "Disable",
        "Download entire video",
        "Download intro chapter",
        "Download first 5 minutes",
        "Download last 5 minutes",
        "Custom chapter regex",
        "Custom time range"
    });
    downloadSectionsComboBox->setCurrentIndex(0);
    downloadSectionsComboBox->setFixedHeight(20);
    downloadSectionsComboBox->setToolTip("Select a preset or custom option for download sections");
    downloadSectionsLayout->addWidget(downloadSectionsComboBox);

    downloadSectionsTextBox = new QLineEdit;
    downloadSectionsTextBox->setPlaceholderText("Input for custom options");
    downloadSectionsTextBox->setEnabled(false);
    downloadSectionsLayout->addWidget(downloadSectionsTextBox);
    downloadSectionsLayout->addStretch();
    generalLayout->addLayout(downloadSectionsLayout);

    connect(downloadSectionsComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index){
        QString selected = downloadSectionsComboBox->itemText(index);
        if (selected == "Custom chapter regex" || selected == "Custom time range") {
            downloadSectionsTextBox->setEnabled(true);
            if (selected == "Custom chapter regex") {
                downloadSectionsTextBox->setPlaceholderText("Enter chapter regex pattern");
            } else {
                downloadSectionsTextBox->setPlaceholderText("Enter time range: 0:00-5:00 or 5:00-inf");
            }
        } else {
            downloadSectionsTextBox->setEnabled(false);
            downloadSectionsTextBox->clear();
        }
        updateCommandPreview();
    });
    connect(downloadSectionsTextBox, &QLineEdit::textChanged, this, &MainWindow::updateCommandPreview);

    generalLayout->addStretch();
    generalTab->setLayout(generalLayout);
    tabWidget->addTab(generalTab, "General");

    // Regex Tab
    QWidget *regexTab = new QWidget;
    QVBoxLayout *regexLayout = new QVBoxLayout;
    regexLayout->setSpacing(4);
    regexLayout->setContentsMargins(5, 5, 5, 5);

    downloadAdditionalUrlsCheck = new QCheckBox("Download additional videos present in video metadata");
    downloadAdditionalUrlsCheck->setToolTip("Extract and download additional videos from video metadata using a regex pattern");
    connect(downloadAdditionalUrlsCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    connect(downloadAdditionalUrlsCheck, &QCheckBox::toggled, [this](bool checked) {
        additionalUrlsRegexTextBox->setEnabled(checked);
        regexSourceLabel->setEnabled(checked);
        videoUrlsRegexComboBox->setEnabled(checked);
        playlistUrlsRegexComboBox->setEnabled(checked);
        channelUrlsRegexComboBox->setEnabled(checked);
        mobileUrlsRegexComboBox->setEnabled(checked);
        if (!checked) {
            videoUrlsRegexComboBox->blockSignals(true);
            playlistUrlsRegexComboBox->blockSignals(true);
            channelUrlsRegexComboBox->blockSignals(true);
            mobileUrlsRegexComboBox->blockSignals(true);
            videoUrlsRegexComboBox->setCurrentIndex(0);
            playlistUrlsRegexComboBox->setCurrentIndex(0);
            channelUrlsRegexComboBox->setCurrentIndex(0);
            mobileUrlsRegexComboBox->setCurrentIndex(0);
            additionalUrlsRegexTextBox->clear();
            regexSourceLabel->setText("Regex source: None");
            videoUrlsRegexComboBox->blockSignals(false);
            playlistUrlsRegexComboBox->blockSignals(false);
            channelUrlsRegexComboBox->blockSignals(false);
            mobileUrlsRegexComboBox->blockSignals(false);
        }
    });
    regexLayout->addWidget(downloadAdditionalUrlsCheck);

    regexSourceLabel = new QLabel("Regex source: None");
    regexSourceLabel->setToolTip("Indicates the source of the regex pattern in the text box below");
    regexSourceLabel->setEnabled(false);
    regexLayout->addWidget(regexSourceLabel);

    additionalUrlsRegexTextBox = new QLineEdit;
    additionalUrlsRegexTextBox->setPlaceholderText("Manually input or select one from the dropdown menus below");
    additionalUrlsRegexTextBox->setEnabled(false);
    connect(additionalUrlsRegexTextBox, &QLineEdit::textChanged, [this](const QString &text) {
        if (!text.isEmpty()) {
            videoUrlsRegexComboBox->blockSignals(true);
            playlistUrlsRegexComboBox->blockSignals(true);
            channelUrlsRegexComboBox->blockSignals(true);
            mobileUrlsRegexComboBox->blockSignals(true);
            videoUrlsRegexComboBox->setCurrentIndex(0);
            playlistUrlsRegexComboBox->setCurrentIndex(0);
            channelUrlsRegexComboBox->setCurrentIndex(0);
            mobileUrlsRegexComboBox->setCurrentIndex(0);
            regexSourceLabel->setText("Regex source: Custom regex");
            videoUrlsRegexComboBox->blockSignals(false);
            playlistUrlsRegexComboBox->blockSignals(false);
            channelUrlsRegexComboBox->blockSignals(false);
            mobileUrlsRegexComboBox->blockSignals(false);
        } else {
            regexSourceLabel->setText("Regex source: None");
        }
        updateCommandPreview();
    });
    regexLayout->addWidget(additionalUrlsRegexTextBox);

    // Youtube Video urls
    QLabel *videoUrlsLabel = new QLabel("Video URLs:");
    videoUrlsLabel->setToolTip("Regex patterns for YouTube video URLs");
    regexLayout->addWidget(videoUrlsLabel);
    videoUrlsRegexComboBox = new QComboBox;
    videoUrlsRegexComboBox->addItems({
        "Disable",
        "description:(?P<additional_urls>https?://(?:www\\.youtube\\.com/watch\\?v=|youtu\\.be/)[a-zA-Z0-9_.-]{11})",
        "description:(?P<additional_urls>https?://www\\.youtube\\.com/watch\\?v=[a-zA-Z0-9_.-]{11})",
        "description:(?P<additional_urls>https?://youtu\\.be/[a-zA-Z0-9_.-]{11})",
        "description:(?P<additional_urls>https?://(?:www\\.youtube\\.com/watch\\?v=|youtu\\.be/)[a-zA-Z0-9_.-]{11}(?:&[a-zA-Z0-9_=&-]*)*)"
    });
    videoUrlsRegexComboBox->setCurrentIndex(0);
    videoUrlsRegexComboBox->setEnabled(false);
    videoUrlsRegexComboBox->setFixedHeight(20);
    videoUrlsRegexComboBox->setToolTip("Select a regex pattern for YouTube video URLs or Disable");
    connect(videoUrlsRegexComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onRegexComboBoxChanged);
    regexLayout->addWidget(videoUrlsRegexComboBox);

    // Youtube Playlist urls
    QLabel *playlistUrlsLabel = new QLabel("Playlist URLs:");
    playlistUrlsLabel->setToolTip("Regex pattern for YouTube playlist URLs");
    regexLayout->addWidget(playlistUrlsLabel);
    playlistUrlsRegexComboBox = new QComboBox;
    playlistUrlsRegexComboBox->addItems({
        "Disable",
        "description:(?P<additional_urls>https?://www\\.youtube\\.com/playlist\\?list=[a-zA-Z0-9_.-]+)"
    });
    playlistUrlsRegexComboBox->setCurrentIndex(0);
    playlistUrlsRegexComboBox->setEnabled(false);
    playlistUrlsRegexComboBox->setFixedHeight(20);
    playlistUrlsRegexComboBox->setToolTip("Select a regex pattern for YouTube playlist URLs or Disable");
    connect(playlistUrlsRegexComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onRegexComboBoxChanged);
    regexLayout->addWidget(playlistUrlsRegexComboBox);

    // Youtube Channel urls
    QLabel *channelUrlsLabel = new QLabel("Channel URLs:");
    channelUrlsLabel->setToolTip("Regex pattern for YouTube channel URLs");
    regexLayout->addWidget(channelUrlsLabel);
    channelUrlsRegexComboBox = new QComboBox;
    channelUrlsRegexComboBox->addItems({
        "Disable",
        "description:(?P<additional_urls>https?://www\\.youtube\\.com/(?:channel/[a-zA-Z0-9_.-]+|@[a-zA-Z0-9_.-]+))"
    });
    channelUrlsRegexComboBox->setCurrentIndex(0);
    channelUrlsRegexComboBox->setEnabled(false);
    channelUrlsRegexComboBox->setFixedHeight(20);
    channelUrlsRegexComboBox->setToolTip("Select a regex pattern for YouTube channel URLs or Disable");
    connect(channelUrlsRegexComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onRegexComboBoxChanged);
    regexLayout->addWidget(channelUrlsRegexComboBox);

    // Youtube Mobile/Short urls
    QLabel *mobileUrlsLabel = new QLabel("Mobile/Short URLs:");
    mobileUrlsLabel->setToolTip("Regex pattern for YouTube mobile or short URLs");
    regexLayout->addWidget(mobileUrlsLabel);
    mobileUrlsRegexComboBox = new QComboBox;
    mobileUrlsRegexComboBox->addItems({
        "Disable",
        "description:(?P<additional_urls>https?://(?:www\\.youtube\\.com|m\\.youtube\\.com)/watch\\?v=[a-zA-Z0-9_.-]{11})"
    });
    mobileUrlsRegexComboBox->setCurrentIndex(0);
    mobileUrlsRegexComboBox->setEnabled(false);
    mobileUrlsRegexComboBox->setFixedHeight(20);
    mobileUrlsRegexComboBox->setToolTip("Select a regex pattern for YouTube mobile or short URLs or Disable");
    connect(mobileUrlsRegexComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onRegexComboBoxChanged);
    regexLayout->addWidget(mobileUrlsRegexComboBox);

    regexLayout->addStretch();
    regexTab->setLayout(regexLayout);
    tabWidget->addTab(regexTab, "Regex");

    // SponsorBlock Tab
    QWidget *sponsorBlockTab = new QWidget;
    QVBoxLayout *sponsorBlockLayout = new QVBoxLayout;
    sponsorBlockLayout->setSpacing(4);
    sponsorBlockLayout->setContentsMargins(5, 5, 5, 5);

    enableSponsorBlockCheck = new QCheckBox("Enable SponsorBlock");
    enableSponsorBlockCheck->setChecked(true);
    enableSponsorBlockCheck->setToolTip("Enables SponsorBlock to remove unwanted segments");
    connect(enableSponsorBlockCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    sponsorBlockLayout->addWidget(enableSponsorBlockCheck);

    sponsorCheck = new QCheckBox("Block Sponsors");
    sponsorCheck->setChecked(true);
    sponsorCheck->setEnabled(true);
    sponsorCheck->setToolTip("Removes sponsor segments");
    sponsorCheck->setEnabled(true);
    connect(enableSponsorBlockCheck, &QCheckBox::toggled, sponsorCheck, &QCheckBox::setEnabled);
    connect(sponsorCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    sponsorBlockLayout->addWidget(sponsorCheck);

    selfPromoCheck = new QCheckBox("Block Self Promotions");
    selfPromoCheck->setToolTip("Removes self promotion segments");
    selfPromoCheck->setEnabled(true);
    connect(enableSponsorBlockCheck, &QCheckBox::toggled, selfPromoCheck, &QCheckBox::setEnabled);
    connect(selfPromoCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    sponsorBlockLayout->addWidget(selfPromoCheck);

    interactionCheck = new QCheckBox("Block Interaction Reminders");
    interactionCheck->setToolTip("Removes calls to like / subscribe");
    interactionCheck->setEnabled(true);
    connect(enableSponsorBlockCheck, &QCheckBox::toggled, interactionCheck, &QCheckBox::setEnabled);
    connect(interactionCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    sponsorBlockLayout->addWidget(interactionCheck);

    fillerCheck = new QCheckBox("Block Filler Content");
    fillerCheck->setToolTip("Removes pointless filler content");
    fillerCheck->setEnabled(true);
    connect(enableSponsorBlockCheck, &QCheckBox::toggled, fillerCheck, &QCheckBox::setEnabled);
    connect(fillerCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    sponsorBlockLayout->addWidget(fillerCheck);

    outroCheck = new QCheckBox("Block Credits");
    outroCheck->setToolTip("Removes ending credits");
    outroCheck->setEnabled(true);
    connect(enableSponsorBlockCheck, &QCheckBox::toggled, outroCheck, &QCheckBox::setEnabled);
    connect(outroCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    sponsorBlockLayout->addWidget(outroCheck);

    introCheck = new QCheckBox("Block Introductions");
    introCheck->setToolTip("Skips introduction part of the video");
    introCheck->setEnabled(true);
    connect(enableSponsorBlockCheck, &QCheckBox::toggled, introCheck, &QCheckBox::setEnabled);
    connect(introCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    sponsorBlockLayout->addWidget(introCheck);

    previewCheck = new QCheckBox("Block Previews");
    previewCheck->setToolTip("Skips preview segments");
    previewCheck->setEnabled(true);
    connect(enableSponsorBlockCheck, &QCheckBox::toggled, previewCheck, &QCheckBox::setEnabled);
    connect(previewCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    sponsorBlockLayout->addWidget(previewCheck);

    musicOfftopicCheck = new QCheckBox("Block Offtopic Music");
    musicOfftopicCheck->setToolTip("Removes unrelated background music");
    musicOfftopicCheck->setEnabled(true);
    connect(enableSponsorBlockCheck, &QCheckBox::toggled, musicOfftopicCheck, &QCheckBox::setEnabled);
    connect(musicOfftopicCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    sponsorBlockLayout->addWidget(musicOfftopicCheck);

    sponsorBlockLayout->addStretch();
    sponsorBlockTab->setLayout(sponsorBlockLayout);
    tabWidget->addTab(sponsorBlockTab, "SponsorBlock");

    // Authorization Tab
    QWidget *authTab = new QWidget;
    QVBoxLayout *authLayout = new QVBoxLayout;
    authLayout->setSpacing(4);
    authLayout->setContentsMargins(5, 5, 5, 5);

    enableAuthCheck = new QCheckBox("Enable Authorization");
    enableAuthCheck->setToolTip("Enable username and password for sites that need login");
    connect(enableAuthCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    authLayout->addWidget(enableAuthCheck);

    QLabel *usernameLabel = new QLabel("Username:");
    authLayout->addWidget(usernameLabel);
    usernameTextBox = new QLineEdit;
    usernameTextBox->setEnabled(false);
    authLayout->addWidget(usernameTextBox);
    connect(enableAuthCheck, &QCheckBox::toggled, usernameTextBox, &QLineEdit::setEnabled);
    connect(usernameTextBox, &QLineEdit::textChanged, this, &MainWindow::updateCommandPreview);

    QLabel *passwordLabel = new QLabel("Password:");
    authLayout->addWidget(passwordLabel);
    passwordTextBox = new QLineEdit;
    passwordTextBox->setEnabled(false);
    authLayout->addWidget(passwordTextBox);
    connect(enableAuthCheck, &QCheckBox::toggled, passwordTextBox, &QLineEdit::setEnabled);
    connect(passwordTextBox, &QLineEdit::textChanged, this, &MainWindow::updateCommandPreview);

    // Cookies File
    useCookiesFileCheck = new QCheckBox("Use Cookies File");
    useCookiesFileCheck->setToolTip("Use a cookies file for authentication");
    authLayout->addWidget(useCookiesFileCheck);

    QHBoxLayout *cookiesFileLayout = new QHBoxLayout;
    cookiesFileTextBox = new QLineEdit;
    cookiesFileTextBox->setPlaceholderText("Path to cookies file");
    cookiesFileLayout->addWidget(cookiesFileTextBox);
    browseCookiesFileButton = new QPushButton("Browse");
    browseCookiesFileButton->setEnabled(false);
    connect(browseCookiesFileButton, &QPushButton::clicked, this, &MainWindow::onBrowseCookiesFileClicked);
    cookiesFileLayout->addWidget(browseCookiesFileButton);
    authLayout->addLayout(cookiesFileLayout);

    // Cookies from Browser
    extractCookiesFromBrowserCheck = new QCheckBox("Extract Cookies from Browser");
    extractCookiesFromBrowserCheck->setToolTip("Extract cookies from a specified browser");
    authLayout->addWidget(extractCookiesFromBrowserCheck);

    browserComboBox = new QComboBox;
    browserComboBox->addItems({"Brave", "Chrome", "Firefox"});
    browserComboBox->setEnabled(false);
    QHBoxLayout *browserLayout = new QHBoxLayout;
    browserLayout->addWidget(browserComboBox);
    browserProfileTextBox = new QLineEdit;
    browserProfileTextBox->setPlaceholderText("Optional: Manually input user data directory");
    connect(browserComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        QString browser = browserComboBox->itemText(index).toLower();
        if (browser == "firefox") {
            browserProfileTextBox->setPlaceholderText("Optional: Manually input profile path or name");
        } else if (browser == "chrome" || browser == "brave") {
            browserProfileTextBox->setPlaceholderText("Optional: Manually input user data directory");
        } else {
            browserProfileTextBox->setPlaceholderText("Optional: Manually input profile Path");
        }
        updateCommandPreview();
    });
    connect(browserProfileTextBox, &QLineEdit::textChanged, this, &MainWindow::updateCommandPreview);
    browserProfileTextBox->setEnabled(false);
    browserLayout->addWidget(browserProfileTextBox);
    authLayout->addLayout(browserLayout);

    connect(useCookiesFileCheck, &QCheckBox::toggled, this, &MainWindow::onUseCookiesFileToggled);
    connect(extractCookiesFromBrowserCheck, &QCheckBox::toggled, this, &MainWindow::onExtractCookiesFromBrowserToggled);
    connect(cookiesFileTextBox, &QLineEdit::textChanged, this, &MainWindow::updateCommandPreview);
    connect(browserComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateCommandPreview);

    authLayout->addStretch();
    authTab->setLayout(authLayout);
    tabWidget->addTab(authTab, "Authorization");

    // List Formats Tab
    QWidget *listFormatsTab = new QWidget;
    QVBoxLayout *listFormatsLayout = new QVBoxLayout;
    listFormatsLayout->setSpacing(4);
    listFormatsLayout->setContentsMargins(5, 5, 5, 5);

    listFormatsButton = new QPushButton("List Formats");
    connect(listFormatsButton, &QPushButton::clicked, this, &MainWindow::onListFormatsClicked);
    listFormatsLayout->addWidget(listFormatsButton);

    formatsTextEdit = new QTextEdit;
    formatsTextEdit->setReadOnly(true);
    formatsTextEdit->setFontFamily("sans-serif");
    listFormatsLayout->addWidget(formatsTextEdit);

    QHBoxLayout *formatCodeLayout = new QHBoxLayout;
    QLabel *formatCodeLabel = new QLabel("Selected Format Code:");
    formatCodeLayout->addWidget(formatCodeLabel);
    selectedFormatCodeTextBox = new QLineEdit;
    selectedFormatCodeTextBox->setPlaceholderText("example: bestvideo+bestaudio");
    formatCodeLayout->addWidget(selectedFormatCodeTextBox);
    QLabel *formatCodeInfo = new QLabel("example: 22 or 137+140 or 137,140");
    formatCodeLayout->addWidget(formatCodeInfo);
    listFormatsLayout->addLayout(formatCodeLayout);
    QHBoxLayout *quickFormatsLayout = new QHBoxLayout;
    useQuickFormatsCheck = new QCheckBox("Use Quick Formats");
    quickFormatsComboBox = new QComboBox;
    quickFormatsComboBox->addItems({
        "bestvideo[height=720]+bestaudio",
        "bestvideo[height=1080]+bestaudio",
        "bestvideo[height=1440]+bestaudio",
        "bestvideo[height=2160]+bestaudio",
        "bestvideo[height<=720]+bestaudio/best",
        "bestvideo[height<=1080]+bestaudio/best",
        "bestvideo[height<=1440]+bestaudio/best",
        "bestvideo[height<=2160]+bestaudio/best",
        "bestvideo[height>=720]+bestaudio/best",
        "bestvideo[height>=1080]+bestaudio/best",
        "bestvideo[height>=1440]+bestaudio/best",
        "bestvideo[height>=2160]+bestaudio/best",
        "bestvideo",
        "bestaudio"
    });
    quickFormatsComboBox->setCurrentIndex(4);
    quickFormatsComboBox->setEnabled(false);
    connect(useQuickFormatsCheck, &QCheckBox::toggled, quickFormatsComboBox, &QComboBox::setEnabled);
    connect(useQuickFormatsCheck, &QCheckBox::toggled, [this](bool checked) {
        if (checked) {
            selectedFormatCodeTextBox->setText(quickFormatsComboBox->currentText());
        } else {
            selectedFormatCodeTextBox->clear();
        }
        updateCommandPreview();
    });
    connect(quickFormatsComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        if (useQuickFormatsCheck->isChecked()) {
            selectedFormatCodeTextBox->setText(quickFormatsComboBox->itemText(index));
        }
        updateCommandPreview();
    });
    quickFormatsLayout->addWidget(useQuickFormatsCheck);
    quickFormatsLayout->addWidget(quickFormatsComboBox);
    quickFormatsLayout->addStretch();
    listFormatsLayout->addLayout(quickFormatsLayout);

    listFormatsTab->setLayout(listFormatsLayout);
    tabWidget->addTab(listFormatsTab, "List Formats");

    // Playlist Tab
    QWidget *playlistTab = new QWidget;
    QVBoxLayout *playlistLayout = new QVBoxLayout;
    playlistLayout->setSpacing(4);
    playlistLayout->setContentsMargins(5, 5, 5, 5);

    // Search
    QHBoxLayout *playlistSearchLayout = new QHBoxLayout;
    QLabel *playlistSearchLabel = new QLabel("Search Videos:");
    playlistSearchLayout->addWidget(playlistSearchLabel);
    playlistSearchTextBox = new QLineEdit;
    playlistSearchTextBox->setPlaceholderText("Enter search term");
    playlistSearchTextBox->setFixedHeight(20);
    playlistSearchLayout->addWidget(playlistSearchTextBox);
    QPushButton *playlistClearSearchButton = new QPushButton("Clear");
    playlistClearSearchButton->setFixedHeight(20);
    playlistSearchLayout->addWidget(playlistClearSearchButton);
    playlistLayout->addLayout(playlistSearchLayout);
    connect(playlistSearchTextBox, &QLineEdit::textChanged, this, &MainWindow::onPlaylistSearchTextChanged);
    connect(playlistClearSearchButton, &QPushButton::clicked, playlistSearchTextBox, &QLineEdit::clear);

    // The list
    playlistListWidget = new QListWidget;
    QFont font;
    font.setFamily("sans-serif");
    font.setStyleHint(QFont::SansSerif);
    playlistListWidget->setFont(font);
    playlistLayout->addWidget(playlistListWidget);

    // Checkbox for using selected items
    useSelectedItemsCheck = new QCheckBox("Use selected items for download");
    useSelectedItemsCheck->setToolTip("Download only the selected items from the playlist");
    connect(useSelectedItemsCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    playlistLayout->addWidget(useSelectedItemsCheck);

    // Select All, Deselect All, and List Videos buttons
    QHBoxLayout *selectButtonsLayout = new QHBoxLayout;
    QPushButton *selectAllButton = new QPushButton("Select All");
    QPushButton *deselectAllButton = new QPushButton("Deselect All");
    listPlaylistButton = new QPushButton("List Videos");
    listPlaylistButton->setToolTip("List titles of videos in the playlist");
    connect(listPlaylistButton, &QPushButton::clicked, this, &MainWindow::onListPlaylistClicked);
    selectButtonsLayout->addWidget(selectAllButton);
    selectButtonsLayout->addWidget(deselectAllButton);
    selectButtonsLayout->addWidget(listPlaylistButton);
    selectButtonsLayout->addStretch();
    playlistLayout->addLayout(selectButtonsLayout);

    connect(selectAllButton, &QPushButton::clicked, [this]() {
        for (int i = 0; i < playlistListWidget->count(); ++i) {
            QListWidgetItem *item = playlistListWidget->item(i);
            if (item->flags() & Qt::ItemIsUserCheckable) {
                item->setCheckState(Qt::Checked);
            }
        }
    });
    connect(deselectAllButton, &QPushButton::clicked, [this]() {
        for (int i = 0; i < playlistListWidget->count(); ++i) {
            QListWidgetItem *item = playlistListWidget->item(i);
            if (item->flags() & Qt::ItemIsUserCheckable) {
                item->setCheckState(Qt::Unchecked);
            }
        }
    });

    playlistLayout->addStretch();
    playlistTab->setLayout(playlistLayout);
    tabWidget->addTab(playlistTab, "Playlist");

    // Channel Browser Tab
    QWidget *channelBrowserTab = new QWidget;
    QVBoxLayout *channelBrowserLayout = new QVBoxLayout;
    channelBrowserLayout->setSpacing(4);
    channelBrowserLayout->setContentsMargins(5, 5, 5, 5);

    // Search
    QHBoxLayout *channelSearchLayout = new QHBoxLayout;
    QLabel *channelSearchLabel = new QLabel("Search Videos:");
    channelSearchLayout->addWidget(channelSearchLabel);
    channelSearchTextBox = new QLineEdit;
    channelSearchTextBox->setPlaceholderText("Enter search term");
    channelSearchTextBox->setFixedHeight(20);
    channelSearchLayout->addWidget(channelSearchTextBox);
    QPushButton *channelClearSearchButton = new QPushButton("Clear");
    channelClearSearchButton->setFixedHeight(20);
    channelSearchLayout->addWidget(channelClearSearchButton);
    channelBrowserLayout->addLayout(channelSearchLayout);
    connect(channelSearchTextBox, &QLineEdit::textChanged, this, &MainWindow::onChannelSearchTextChanged);
    connect(channelClearSearchButton, &QPushButton::clicked, channelSearchTextBox, &QLineEdit::clear);

    // The one true list
    channelListWidget = new QListWidget;
    channelListWidget->setFont(font);
    connect(channelListWidget, &QListWidget::itemChanged, this, &MainWindow::updateUseSelectedChannelCheck);
    channelBrowserLayout->addWidget(channelListWidget);

    // Content type dropdown
    QHBoxLayout *contentTypeLayout = new QHBoxLayout;
    QLabel *contentTypeLabel = new QLabel("Content Type:");
    contentTypeLayout->addWidget(contentTypeLabel);
    channelContentComboBox = new QComboBox;
    channelContentComboBox->addItems({"Videos", "Shorts", "Live Streams"});
    channelContentComboBox->setFixedHeight(20);
    channelContentComboBox->setToolTip("Select the type of content to browse");
    contentTypeLayout->addWidget(channelContentComboBox);
    contentTypeLayout->addStretch();
    videoCountLabel = new QLabel("0 listed");
    videoCountLabel->setFixedHeight(20);
    videoCountLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    contentTypeLayout->addWidget(videoCountLabel);
    channelBrowserLayout->addLayout(contentTypeLayout);

    // List limit dropdown
    QHBoxLayout *listLimitLayout = new QHBoxLayout;
    QLabel *listLimitLabel = new QLabel("List:");
    listLimitComboBox = new QComboBox;
    listLimitComboBox->addItems({"All", "1000", "500", "250", "100", "50", "25", "10", "5", "1"});
    listLimitComboBox->setCurrentIndex(0);
    listLimitComboBox->setFixedHeight(20);
    listLimitComboBox->setToolTip("Select how many videos to list");
    listLimitLayout->addWidget(listLimitLabel);
    listLimitLayout->addWidget(listLimitComboBox);
    listLimitLayout->addStretch();
    channelBrowserLayout->addLayout(listLimitLayout);

    // Checkbox for using selected items
    useSelectedChannelItemsCheck = new QCheckBox("Use selected items for download");
    useSelectedChannelItemsCheck->setToolTip("Download only the selected items from the channel");
    connect(useSelectedChannelItemsCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    channelBrowserLayout->addWidget(useSelectedChannelItemsCheck);

    // New checkbox for showing upload dates
    showUploadDatesCheck = new QCheckBox("Show upload dates (much slower listing)");
    showUploadDatesCheck->setChecked(false);
    showUploadDatesCheck->setToolTip("Enable to include upload dates in the list, but listing will be drastically slower due to yt-dlp processing. ~4 minutes for 100 vs ~2 seconds without dates");
    channelBrowserLayout->addWidget(showUploadDatesCheck);
    connect(showUploadDatesCheck, &QCheckBox::toggled, this, &MainWindow::onListChannelClicked);

    connect(channelContentComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onListChannelClicked);
    connect(listLimitComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onListChannelClicked);

    // Select All, Deselect All, and List Videos buttons
    QHBoxLayout *channelSelectButtonsLayout = new QHBoxLayout;
    QPushButton *channelSelectAllButton = new QPushButton("Select All");
    QPushButton *channelDeselectAllButton = new QPushButton("Deselect All");
    listChannelButton = new QPushButton("List Videos");
    listChannelButton->setToolTip("List titles of videos, shorts, or streams in the channel");
    connect(listChannelButton, &QPushButton::clicked, this, &MainWindow::onListChannelClicked);
    channelSelectButtonsLayout->addWidget(channelSelectAllButton);
    channelSelectButtonsLayout->addWidget(channelDeselectAllButton);
    channelSelectButtonsLayout->addWidget(listChannelButton);
    channelSelectButtonsLayout->addStretch();
    channelBrowserLayout->addLayout(channelSelectButtonsLayout);

    channelBrowserLayout->addSpacerItem(new QSpacerItem(0, 10, QSizePolicy::Minimum, QSizePolicy::Fixed));

    // Bookmarks section
    QLabel *bookmarksLabel = new QLabel("Bookmarked Channels:");
    channelBrowserLayout->addWidget(bookmarksLabel);

    bookmarksTable = new QTableWidget;
    bookmarksTable->setColumnCount(8);
    bookmarksTable->setHorizontalHeaderLabels(QStringList() << "Name" << "URL" << "Output Directory" << "Filename Format" << "Output Dir Format" << "Subdirectory" << "List Limit" << "Selected Format Code");
    bookmarksTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    bookmarksTable->setSelectionMode(QAbstractItemView::SingleSelection);
    bookmarksTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    bookmarksTable->verticalHeader()->hide();
    bookmarksTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    channelBrowserLayout->addWidget(bookmarksTable);

    QHBoxLayout *bookmarksButtonsLayout = new QHBoxLayout;
    QPushButton *addBookmarkButton = new QPushButton("Add Bookmark");
    QPushButton *removeBookmarkButton = new QPushButton("Remove Bookmark");
    QPushButton *editBookmarkButton = new QPushButton("Edit Bookmark");
    bookmarksButtonsLayout->addWidget(addBookmarkButton);
    bookmarksButtonsLayout->addWidget(removeBookmarkButton);
    bookmarksButtonsLayout->addWidget(editBookmarkButton);
    bookmarksButtonsLayout->addStretch();
    channelBrowserLayout->addLayout(bookmarksButtonsLayout);

    connect(addBookmarkButton, &QPushButton::clicked, this, &MainWindow::onBookmarkClicked);
    connect(removeBookmarkButton, &QPushButton::clicked, this, &MainWindow::onRemoveBookmarkClicked);
    connect(editBookmarkButton, &QPushButton::clicked, this, &MainWindow::onEditBookmarkClicked);
    connect(bookmarksTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::onBookmarkTableSelectionChanged);

    connect(channelSelectAllButton, &QPushButton::clicked, [this]() {
        for (int i = 0; i < channelListWidget->count(); ++i) {
            QListWidgetItem *item = channelListWidget->item(i);
            if (item->flags() & Qt::ItemIsUserCheckable) {
                item->setCheckState(Qt::Checked);
            }
        }
    });
    connect(channelDeselectAllButton, &QPushButton::clicked, [this]() {
        for (int i = 0; i < channelListWidget->count(); ++i) {
            QListWidgetItem *item = channelListWidget->item(i);
            if (item->flags() & Qt::ItemIsUserCheckable) {
                item->setCheckState(Qt::Unchecked);
            }
        }
    });

    channelBrowserLayout->addStretch();
    channelBrowserTab->setLayout(channelBrowserLayout);
    tabWidget->addTab(channelBrowserTab, "Channel Browser");

    // Subtitles Tab
    QWidget *subtitlesTab = new QWidget;
    QVBoxLayout *subtitlesLayout = new QVBoxLayout;
    subtitlesLayout->setSpacing(4);
    subtitlesLayout->setContentsMargins(5, 5, 5, 5);

    downloadSubsCheck = new QCheckBox("Download Subtitles");
    downloadSubsCheck->setToolTip("Download subtitles for the video");
    connect(downloadSubsCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    subtitlesLayout->addWidget(downloadSubsCheck);

    // Thing to hold the subtitle options
    QWidget *subsOptionsWidget = new QWidget;
    QVBoxLayout *subsOptionsLayout = new QVBoxLayout;
    subsOptionsLayout->setContentsMargins(0, 0, 0, 0);

    // Buttons for languages
    allSubsRadio = new QRadioButton("All languages");
    specificSubsRadio = new QRadioButton("Specific languages");
    QButtonGroup *subsGroup = new QButtonGroup(this);
    subsGroup->addButton(allSubsRadio);
    subsGroup->addButton(specificSubsRadio);
    allSubsRadio->setChecked(true);
    connect(allSubsRadio, &QRadioButton::toggled, this, &MainWindow::updateCommandPreview);
    connect(specificSubsRadio, &QRadioButton::toggled, this, &MainWindow::updateCommandPreview);
    subsOptionsLayout->addWidget(allSubsRadio);
    subsOptionsLayout->addWidget(specificSubsRadio);

    // List for selecting multiple languages
    subsLangList = new QListWidget;
    subsLangList->setSelectionMode(QAbstractItemView::MultiSelection);
    QStringList languages = {
        "en", "es", "fr", "de", "nl", "it", "pt", "el", "ru", "fi", "uk", "sv", "ar", "tr", "ja", "ko", "zh"
    };
    for (const QString& lang : languages) {
        QListWidgetItem *item = new QListWidgetItem(lang);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        subsLangList->addItem(item);
    }
    subsLangList->setEnabled(false);
    connect(specificSubsRadio, &QRadioButton::toggled, subsLangList, &QListWidget::setEnabled);
    connect(subsLangList, &QListWidget::itemChanged, this, &MainWindow::updateCommandPreview);
    subsOptionsLayout->addWidget(subsLangList);

    subsOptionsWidget->setLayout(subsOptionsLayout);
    subsOptionsWidget->setEnabled(false);
    connect(downloadSubsCheck, &QCheckBox::toggled, subsOptionsWidget, &QWidget::setEnabled);
    subtitlesLayout->addWidget(subsOptionsWidget);

    embedSubsCheck = new QCheckBox("Embed Subtitles");
    embedSubsCheck->setToolTip("Embed subtitles into the video file");
    embedSubsCheck->setEnabled(false);
    connect(downloadSubsCheck, &QCheckBox::toggled, embedSubsCheck, &QCheckBox::setEnabled);
    connect(embedSubsCheck, &QCheckBox::toggled, this, &MainWindow::updateCommandPreview);
    subtitlesLayout->addWidget(embedSubsCheck);

    subtitlesLayout->addStretch();
    subtitlesTab->setLayout(subtitlesLayout);
    tabWidget->addTab(subtitlesTab, "Subtitles");

    // Save Config Tab
    QWidget *saveConfigTab = new QWidget;
    QVBoxLayout *saveConfigLayout = new QVBoxLayout;
    saveConfigLayout->setSpacing(4);
    saveConfigLayout->setContentsMargins(5, 5, 5, 5);

    QLabel *saveConfigLabel = new QLabel("Click the button below to save the current settings to yt-dlp's configuration file.");
    saveConfigLayout->addWidget(saveConfigLabel);

    QPushButton *saveConfigButton = new QPushButton("Save Configuration");
    connect(saveConfigButton, &QPushButton::clicked, this, &MainWindow::onSaveConfigClicked);
    saveConfigLayout->addWidget(saveConfigButton);

    saveConfigLayout->addStretch();
    saveConfigTab->setLayout(saveConfigLayout);
    tabWidget->addTab(saveConfigTab, "Save Config");

    // Console Tab
    QWidget *consoleTab = new QWidget;
    QVBoxLayout *consoleLayout = new QVBoxLayout;
    consoleLayout->setSpacing(4);
    consoleLayout->setContentsMargins(5, 5, 5, 5);

    commandPreviewTextBox = new QLineEdit;
    commandPreviewTextBox->setReadOnly(true);
    commandPreviewTextBox->setFixedHeight(25);
    commandPreviewTextBox->setStyleSheet("QLineEdit { background-color: #333333; color: #ffffff; border: 1px solid #555555; padding: 2px; }");
    consoleLayout->addWidget(commandPreviewTextBox);

    consoleTextEdit = new QTextEdit;
    consoleTextEdit->setReadOnly(true);
    consoleTextEdit->setFontFamily("sans-serif");
    consoleTextEdit->setContentsMargins(0, 0, 0, 0);
    consoleLayout->addWidget(consoleTextEdit);

    consoleTab->setLayout(consoleLayout);
    tabWidget->addTab(consoleTab, "Console");

    mainLayout->addWidget(tabWidget);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(2);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addStretch();
    cancelButton = new QPushButton("Cancel");
    cancelButton->setVisible(false);
    cancelButton->setFixedHeight(20);
    cancelButton->setContentsMargins(0, 0, 0, 0);
    connect(cancelButton, &QPushButton::clicked, this, &MainWindow::onCancelClicked);
    buttonLayout->addWidget(cancelButton);
    downloadButton = new QPushButton("Download");
    downloadButton->setFixedHeight(20);
    downloadButton->setContentsMargins(0, 0, 0, 0);
    connect(downloadButton, &QPushButton::clicked, this, &MainWindow::onDownloadClicked);
    buttonLayout->addWidget(downloadButton);
    mainLayout->addLayout(buttonLayout);

    // Progress bar
    progressBar = new QProgressBar;
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setVisible(false);
    progressBar->setFixedHeight(16);
    progressBar->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(progressBar);

    // Set central
    QWidget *centralWidget = new QWidget;
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);

    // Status bar
    statusBar = new QStatusBar;
    statusBar->setSizeGripEnabled(false);
    setStatusBar(statusBar);

    // Set window size
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int width = static_cast<int>(screenGeometry.width() * 0.4297);
    int height = static_cast<int>(screenGeometry.height() * 0.65);
    resize(width, height);
    move((screenGeometry.width() - width) / 2, (screenGeometry.height() - height) / 2);
}

void MainWindow::initializeDatabase() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (configDir.isEmpty()) {
        consoleTextEdit->append("Error: Couldn't determine the config directory.");
        QMessageBox::warning(this, "Configuration Error", "Unable to determine the configuration directory.");
        return;
    }
    QString appConfigDir = configDir + "/ytdlpfrontend";
    QDir dir(appConfigDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            consoleTextEdit->append("Error: Couldn't create the ytdlpfrontend config directory!");
            QMessageBox::warning(this, "Directory Error", "Failed to create the ytdlpfrontend config directory.");
            return;
        }
    }
    QString dbPath = appConfigDir + "/ytdlpf-bookmarks.db";
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        consoleTextEdit->append("Error: Couldn't open database: " + db.lastError().text());
        QMessageBox::warning(this, "Database Error", "Failed to open the bookmark database: " + db.lastError().text());
        return;
    }
    QSqlQuery query;
    if (!query.exec("CREATE TABLE IF NOT EXISTS bookmarks ("
        "id INTEGER PRIMARY KEY, "
        "name TEXT, "
        "url TEXT UNIQUE, "
        "output_dir TEXT, "
        "filename_format TEXT, "
        "output_dir_format TEXT, "
        "use_subdir INTEGER, "
        "subdir_name TEXT)")) {
        consoleTextEdit->append("Error: Couldn't create bookmarks table: " + query.lastError().text());
    QMessageBox::warning(this, "Database Error", "Failed to create bookmarks table: " + query.lastError().text());
    db.close();
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    return;
        }
        QSqlQuery checkQuery;
        checkQuery.exec("PRAGMA table_info(bookmarks)");
        QSet<QString> existingColumns;
        while (checkQuery.next()) {
            existingColumns.insert(checkQuery.value("name").toString());
        }
        QMap<QString, QString> newColumns = {
            {"output_dir", "TEXT"},
            {"filename_format", "TEXT"},
            {"output_dir_format", "TEXT"},
            {"use_subdir", "INTEGER"},
            {"subdir_name", "TEXT"},
            {"list_limit", "TEXT"},
            {"selected_format_code", "TEXT"}
        };
        for (auto it = newColumns.constBegin(); it != newColumns.constEnd(); ++it) {
            if (!existingColumns.contains(it.key())) {
                if (!query.exec(QString("ALTER TABLE bookmarks ADD COLUMN %1 %2").arg(it.key(), it.value()))) {
                    consoleTextEdit->append(QString("Error: Couldn't add %1 column: ").arg(it.key()) + query.lastError().text());
                    QMessageBox::warning(this, "Database Error", QString("Failed to add %1 column: ").arg(it.key()) + query.lastError().text());
                    db.close();
                    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
                    return;
                }
                consoleTextEdit->append(QString("Added %1 column to bookmarks table.").arg(it.key()));
            }
        }
        consoleTextEdit->append("Bookmark database initialized at: " + dbPath);
}

void MainWindow::loadBookmarks() {
    bookmarksTable->clearContents();
    bookmarksTable->setRowCount(0);
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        consoleTextEdit->append("Error: Database is not open for loading bookmarks.");
        QMessageBox::warning(this, "Database Error", "Failed to load bookmarks.");
        return;
    }
    QSqlQuery query;
    if (!query.exec("SELECT name, url, output_dir, filename_format, output_dir_format, use_subdir, subdir_name, list_limit, selected_format_code FROM bookmarks ORDER BY name ASC")) {
        consoleTextEdit->append("Error loading bookmarks: " + query.lastError().text());
        QMessageBox::warning(this, "Database Error", "Failed to load bookmarks: " + query.lastError().text());
        return;
    }
    QMap<QString, QMap<QString, QVariant>> bookmarks;
    while (query.next()) {
        QString name = query.value("name").toString();
        QMap<QString, QVariant> bookmarkData;
        bookmarkData["url"] = query.value("url").toString();
        bookmarkData["output_dir"] = query.value("output_dir").toString();
        bookmarkData["filename_format"] = query.value("filename_format").toString();
        bookmarkData["output_dir_format"] = query.value("output_dir_format").toString();
        bookmarkData["use_subdir"] = query.value("use_subdir").toBool();
        bookmarkData["subdir_name"] = query.value("subdir_name").toString();
        bookmarkData["list_limit"] = query.value("list_limit").toString();
        bookmarkData["selected_format_code"] = query.value("selected_format_code").toString();
        bookmarks.insert(name, bookmarkData);
    }
    bookmarksTable->setRowCount(bookmarks.size());
    bookmarksTable->setColumnCount(8);
    bookmarksTable->setHorizontalHeaderLabels(QStringList() << "Name" << "URL" << "Output Directory" << "Filename Format" << "Output Dir Format" << "Subdirectory" << "List Limit" << "Selected Format Code");
    int row = 0;
    for (auto it = bookmarks.constBegin(); it != bookmarks.constEnd(); ++it) {
        QTableWidgetItem *nameItem = new QTableWidgetItem(it.key());
        QTableWidgetItem *urlItem = new QTableWidgetItem(it.value()["url"].toString());
        QTableWidgetItem *outputDirItem = new QTableWidgetItem(it.value()["output_dir"].toString());
        QTableWidgetItem *filenameFormatItem = new QTableWidgetItem(it.value()["filename_format"].toString());
        QTableWidgetItem *outputDirFormatItem = new QTableWidgetItem(it.value()["output_dir_format"].toString());
        bool useSubdir = it.value()["use_subdir"].toBool();
        QString subdirName = it.value()["subdir_name"].toString();
        QTableWidgetItem *subdirItem = new QTableWidgetItem(useSubdir ? subdirName : "Disabled");
        QString listLimitStr = it.value()["list_limit"].toString();
        QTableWidgetItem *listLimitItem = new QTableWidgetItem(listLimitStr.isEmpty() ? "All" : listLimitStr);
        QString selectedFormatCodeStr = it.value()["selected_format_code"].toString();
        QTableWidgetItem *selectedFormatCodeItem = new QTableWidgetItem(selectedFormatCodeStr.isEmpty() ? "None" : selectedFormatCodeStr);
        bookmarksTable->setItem(row, 0, nameItem);
        bookmarksTable->setItem(row, 1, urlItem);
        bookmarksTable->setItem(row, 2, outputDirItem);
        bookmarksTable->setItem(row, 3, filenameFormatItem);
        bookmarksTable->setItem(row, 4, outputDirFormatItem);
        bookmarksTable->setItem(row, 5, subdirItem);
        bookmarksTable->setItem(row, 6, listLimitItem);
        bookmarksTable->setItem(row, 7, selectedFormatCodeItem);
        row++;
    }
    consoleTextEdit->append("Loaded " + QString::number(bookmarksTable->rowCount()) + " bookmarks.");
    bookmarksTable->resizeColumnsToContents();
}

void MainWindow::onBookmarkClicked() {
    QString url = urlTextBox->text().trimmed();
    if (url.isEmpty()) {
        consoleTextEdit->append("Please enter a channel URL");
        QMessageBox::warning(this, "Empty URL", "Please enter a channel URL.");
        return;
    }
    QRegularExpression channelPathRegex(
        "^(https?://(?:(?:www|m)\\.)?youtube\\.(?:com|co\\.[a-zA-Z]{2})/(?:(?:c/|channel/|user/|@[a-zA-Z0-9_.-]+)(?:/|$)))",
                                        QRegularExpression::CaseInsensitiveOption
    );
    QRegularExpressionMatch match = channelPathRegex.match(url);
    if (!match.hasMatch()) {
        consoleTextEdit->append("Invalid channel URL");
        QMessageBox::warning(this, "Invalid URL",
                             "Please enter a valid YouTube channel URL.\n\n"
                             "Valid examples:\n"
                             "- https://www.youtube.com/@ChannelName\n"
                             "- https://www.youtube.com/channel/UC1234567890\n"
                             "- https://www.youtube.com/c/ChannelName\n"
                             "- https://www.youtube.com/user/Username");
        return;
    }
    QString normalizedUrl = match.captured(1);
    if (normalizedUrl.endsWith('/')) {
        normalizedUrl = normalizedUrl.left(normalizedUrl.length() - 1);
    }

    QString channelName;
    QStringList parts = normalizedUrl.split('/');
    channelName = parts.last();
    if (channelName.startsWith('@')) {
        channelName = channelName.mid(1);
    }

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        consoleTextEdit->append("Error: Database is not open.");
        QMessageBox::warning(this, "Database Error", "Failed to access the bookmark database.");
        return;
    }

    QSqlQuery checkQuery;
    checkQuery.prepare("SELECT COUNT(*) FROM bookmarks WHERE url = :url");
    checkQuery.bindValue(":url", normalizedUrl);
    if (checkQuery.exec() && checkQuery.next() && checkQuery.value(0).toInt() > 0) {
        consoleTextEdit->append("This URL is already bookmarked");
        QMessageBox::warning(this, "Duplicate Bookmark", "This URL is already bookmarked.");
        return;
    }

    QString currentListLimit = listLimitComboBox->currentText();
    QString currentSelectedFormatCode = selectedFormatCodeTextBox->text().trimmed();
    BookmarkDialog dialog(normalizedUrl, channelName, outputDirTextBox->text(),
                          customFormatTextBox->text(),
                          customOutputDirFormatTextBox->text(),
                          useOutputSubdirCheck->isChecked(),
                          outputSubdirTextBox->text(),
                          currentListLimit,
                          currentSelectedFormatCode, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString name = dialog.getName();
        if (name.isEmpty()) {
            consoleTextEdit->append("Bookmark name cannot be empty");
            QMessageBox::warning(this, "Empty Name", "Please enter a name for the bookmark.");
            return;
        }
        QString outputDir = dialog.getOutputDir();
        QString filenameFormat = dialog.getFilenameFormat();
        QString outputDirFormat = dialog.getOutputDirFormat();
        bool useSubdir = dialog.getUseSubdir();
        QString subdirName = dialog.getSubdirName();
        QSqlQuery query;
        query.prepare("INSERT INTO bookmarks (name, url, output_dir, filename_format, output_dir_format, use_subdir, subdir_name, list_limit, selected_format_code) "
        "VALUES (:name, :url, :output_dir, :filename_format, :output_dir_format, :use_subdir, :subdir_name, :list_limit, :selected_format_code)");
        query.bindValue(":name", name);
        query.bindValue(":url", normalizedUrl);
        query.bindValue(":output_dir", outputDir);
        query.bindValue(":filename_format", filenameFormat);
        query.bindValue(":output_dir_format", outputDirFormat);
        query.bindValue(":use_subdir", useSubdir ? 1 : 0);
        query.bindValue(":subdir_name", subdirName);
        query.bindValue(":list_limit", dialog.getListLimit());
        query.bindValue(":selected_format_code", dialog.getSelectedFormatCode());
        if (!query.exec()) {
            consoleTextEdit->append("Error saving bookmark: " + query.lastError().text());
            QMessageBox::warning(this, "Database Error", "Failed to save bookmark: " + query.lastError().text());
        } else {
            QString message = "Bookmarked: " + name;
            if (!outputDir.isEmpty()) message += " with output directory: " + outputDir;
            if (!filenameFormat.isEmpty()) message += ", filename format: " + filenameFormat;
            if (!outputDirFormat.isEmpty()) message += ", output dir format: " + outputDirFormat;
            if (useSubdir) message += ", subdir: " + subdirName;
            QString listLimitMsg = dialog.getListLimit();
            if (!listLimitMsg.isEmpty() && listLimitMsg != "All") message += ", list limit: " + listLimitMsg;
            QString selectedFormatCodeMsg = dialog.getSelectedFormatCode();
            if (!selectedFormatCodeMsg.isEmpty()) message += ", selected format code: " + selectedFormatCodeMsg;
            consoleTextEdit->append(message);
            statusBar->showMessage("Bookmarked: " + name, 5000);
            loadBookmarks();
        }
    }
}

void MainWindow::onRemoveBookmarkClicked() {
    QList<QTableWidgetItem*> selectedItems = bookmarksTable->selectedItems();
    if (selectedItems.isEmpty()) {
        consoleTextEdit->append("No bookmark selected to remove");
        QMessageBox::warning(this, "No Selection", "Please select a bookmark to remove.");
        return;
    }
    int row = selectedItems.first()->row();
    QString url = bookmarksTable->item(row, 1)->text();
    QString name = bookmarksTable->item(row, 0)->text();
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        consoleTextEdit->append("Error: Database is not open.");
        QMessageBox::warning(this, "Database Error", "Failed to access the bookmark database.");
        return;
    }
    QSqlQuery query;
    query.prepare("DELETE FROM bookmarks WHERE url = :url");
    query.bindValue(":url", url);
    if (!query.exec()) {
        consoleTextEdit->append("Error removing bookmark: " + query.lastError().text());
        QMessageBox::warning(this, "Database Error", "Failed to remove bookmark: " + query.lastError().text());
    } else {
        consoleTextEdit->append("Removed bookmark: " + name);
        statusBar->showMessage("Removed bookmark: " + name, 5000);
        loadBookmarks();
    }
}

void MainWindow::onEditBookmarkClicked() {
    QList<QTableWidgetItem*> selectedItems = bookmarksTable->selectedItems();
    if (selectedItems.isEmpty()) {
        consoleTextEdit->append("No bookmark selected to edit");
        QMessageBox::warning(this, "No Selection", "Please select a bookmark to edit.");
        return;
    }
    int row = selectedItems.first()->row();
    QString currentName = bookmarksTable->item(row, 0)->text();
    QString url = bookmarksTable->item(row, 1)->text();
    QString currentOutputDir = bookmarksTable->item(row, 2)->text();
    QSqlQuery query;
    query.prepare("SELECT name, url, output_dir, filename_format, output_dir_format, use_subdir, subdir_name, list_limit, selected_format_code FROM bookmarks WHERE url = :url");
    query.bindValue(":url", url);
    if (query.exec() && query.next()) {
        QString name = query.value("name").toString();
        QString outputDir = query.value("output_dir").toString();
        QString filenameFormat = query.value("filename_format").toString();
        QString outputDirFormat = query.value("output_dir_format").toString();
        bool useSubdir = query.value("use_subdir").toBool();
        QString subdirName = query.value("subdir_name").toString();
        QString listLimit = query.value("list_limit").toString();
        QString selectedFormatCode = query.value("selected_format_code").toString();
        BookmarkDialog dialog(url, name, outputDir, filenameFormat, outputDirFormat, useSubdir, subdirName, listLimit, selectedFormatCode, this);
        if (dialog.exec() == QDialog::Accepted) {
            QString newName = dialog.getName();
            if (newName.isEmpty()) {
                consoleTextEdit->append("Bookmark name cannot be empty");
                QMessageBox::warning(this, "Empty Name", "Bookmark name cannot be empty.");
                return;
            }
            QString newOutputDir = dialog.getOutputDir();
            QString newFilenameFormat = dialog.getFilenameFormat();
            QString newOutputDirFormat = dialog.getOutputDirFormat();
            bool newUseSubdir = dialog.getUseSubdir();
            QString newSubdirName = dialog.getSubdirName();
            query.prepare("UPDATE bookmarks SET name = :name, output_dir = :output_dir, "
            "filename_format = :filename_format, output_dir_format = :output_dir_format, "
            "use_subdir = :use_subdir, subdir_name = :subdir_name, list_limit = :list_limit, selected_format_code = :selected_format_code WHERE url = :url");
            query.bindValue(":name", newName);
            query.bindValue(":output_dir", newOutputDir);
            query.bindValue(":filename_format", newFilenameFormat);
            query.bindValue(":output_dir_format", newOutputDirFormat);
            query.bindValue(":use_subdir", newUseSubdir ? 1 : 0);
            query.bindValue(":subdir_name", newSubdirName);
            query.bindValue(":list_limit", dialog.getListLimit());
            query.bindValue(":selected_format_code", dialog.getSelectedFormatCode());
            query.bindValue(":url", url);
            if (!query.exec()) {
                consoleTextEdit->append("Error updating bookmark: " + query.lastError().text());
                QMessageBox::warning(this, "Database Error", "Failed to update bookmark: " + query.lastError().text());
            } else {
                QString message = "Updated bookmark: " + newName;
                if (!newOutputDir.isEmpty()) message += " with output directory: " + newOutputDir;
                if (!newFilenameFormat.isEmpty()) message += ", filename format: " + newFilenameFormat;
                if (!newOutputDirFormat.isEmpty()) message += ", output dir format: " + newOutputDirFormat;
                if (newUseSubdir) message += ", subdir: " + newSubdirName;
                QString newListLimit = dialog.getListLimit();
                if (!newListLimit.isEmpty() && newListLimit != "All") message += ", list limit: " + newListLimit;
                QString newSelectedFormatCode = dialog.getSelectedFormatCode();
                if (!newSelectedFormatCode.isEmpty()) message += ", selected format code: " + newSelectedFormatCode;
                consoleTextEdit->append(message);
                statusBar->showMessage("Updated bookmark: " + newName, 5000);
                loadBookmarks();
            }
        }
    } else {
        consoleTextEdit->append("Error retrieving bookmark: " + query.lastError().text());
        QMessageBox::warning(this, "Database Error", "Failed to retrieve bookmark: " + query.lastError().text());
    }
}

void MainWindow::onBookmarkTableSelectionChanged() {
    QList<QTableWidgetItem*> selectedItems = bookmarksTable->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }
    int row = selectedItems.first()->row();
    QString url = bookmarksTable->item(row, 1)->text();
    QSqlQuery query;
    query.prepare("SELECT name, url, output_dir, filename_format, output_dir_format, use_subdir, subdir_name, list_limit, selected_format_code FROM bookmarks WHERE url = :url");
    query.bindValue(":url", url);
    if (query.exec() && query.next()) {
        QString outputDir = query.value("output_dir").toString();
        QString filenameFormat = query.value("filename_format").toString();
        QString outputDirFormat = query.value("output_dir_format").toString();
        bool useSubdir = query.value("use_subdir").toBool();
        QString subdirName = query.value("subdir_name").toString();
        QString listLimit = query.value("list_limit").toString();
        QString selectedFormatCode = query.value("selected_format_code").toString();
        urlTextBox->setText(url);
        outputDirTextBox->setText(outputDir);
        customFormatTextBox->setText(filenameFormat.isEmpty() ? formatComboBox->currentText() : filenameFormat);
        customOutputDirFormatTextBox->setText(outputDirFormat);
        if (outputDirFormat.isEmpty()) {
            outputDirFormatComboBox->setCurrentIndex(0);
        } else {
            int index = outputDirFormatComboBox->findText(outputDirFormat);
            outputDirFormatComboBox->setCurrentIndex(index != -1 ? index : 0);
        }
        useOutputSubdirCheck->setChecked(useSubdir);
        outputSubdirTextBox->setText(subdirName.isEmpty() ? "yt-dlp output" : subdirName);
        listLimitComboBox->blockSignals(true);
        int limitIndex = listLimitComboBox->findText(listLimit.isEmpty() ? "All" : listLimit);
        listLimitComboBox->setCurrentIndex(limitIndex != -1 ? limitIndex : 0);
        listLimitComboBox->blockSignals(false);
        selectedFormatCodeTextBox->setText(selectedFormatCode);
        QString message = "Selected bookmark: " + query.value("name").toString();
        if (!outputDir.isEmpty()) message += " with output directory: " + outputDir;
        if (!filenameFormat.isEmpty()) message += ", filename format: " + filenameFormat;
        if (!outputDirFormat.isEmpty()) message += ", output dir format: " + outputDirFormat;
        if (useSubdir) message += ", subdir: " + subdirName;
        if (!listLimit.isEmpty() && listLimit != "All") message += ", list limit: " + listLimit;
        if (!selectedFormatCode.isEmpty()) message += ", selected format code: " + selectedFormatCode;
        consoleTextEdit->append(message);
        statusBar->showMessage("Selected bookmark: " + query.value("name").toString(), 5000);
        updateCommandPreview();
        onListChannelClicked();
    } else {
        consoleTextEdit->append("Error retrieving bookmark: " + query.lastError().text());
        QMessageBox::warning(this, "Database Error", "Failed to retrieve bookmark: " + query.lastError().text());
    }
}

void MainWindow::onRegexComboBoxChanged(int index) {
    QComboBox *senderComboBox = qobject_cast<QComboBox*>(sender());
    if (!senderComboBox) return;

    QComboBox *comboBoxes[] = {videoUrlsRegexComboBox, playlistUrlsRegexComboBox, channelUrlsRegexComboBox, mobileUrlsRegexComboBox};
    for (QComboBox *comboBox : comboBoxes) {
        if (comboBox != senderComboBox) {
            comboBox->blockSignals(true);
            comboBox->setCurrentIndex(0);
            comboBox->blockSignals(false);
        }
    }

    if (index != 0) {
        additionalUrlsRegexTextBox->blockSignals(true);
        additionalUrlsRegexTextBox->setText(senderComboBox->itemText(index));
        additionalUrlsRegexTextBox->blockSignals(false);
        if (senderComboBox == videoUrlsRegexComboBox) {
            regexSourceLabel->setText("Regex source: Video URLs");
        } else if (senderComboBox == playlistUrlsRegexComboBox) {
            regexSourceLabel->setText("Regex source: Playlist URLs");
        } else if (senderComboBox == channelUrlsRegexComboBox) {
            regexSourceLabel->setText("Regex source: Channel URLs");
        } else if (senderComboBox == mobileUrlsRegexComboBox) {
            regexSourceLabel->setText("Regex source: Mobile/Short URLs");
        }
    } else {
        additionalUrlsRegexTextBox->blockSignals(true);
        additionalUrlsRegexTextBox->clear();
        additionalUrlsRegexTextBox->blockSignals(false);
        regexSourceLabel->setText("Regex source: None");
    }

    updateCommandPreview();
}

void MainWindow::onSelectFolderClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Output Directory", outputDirTextBox->text());
    if (!dir.isEmpty()) {
        outputDirTextBox->setText(dir);
        updateCommandPreview();
    }
}

void MainWindow::onDownloadClicked() {
    if (urlTextBox->text().trimmed().isEmpty()) {
        consoleTextEdit->append("Please enter at least one URL");
        statusBar->showMessage("Please enter at least one URL", 5000);
        return;
    }
    QString outputDir = outputDirTextBox->text().trimmed();
    if (outputDir.isEmpty()) {
        consoleTextEdit->append("Please select an output directory");
        statusBar->showMessage("Please select an output directory", 5000);
        return;
    }
    if (!QFileInfo(outputDir).exists()) {
        consoleTextEdit->append("Error: The specified output directory does not exist!");
        statusBar->showMessage("Error: Output directory does not exist!", 5000);
        return;
    }
    QString finalOutputDir = outputDir;
    if (useOutputSubdirCheck->isChecked()) {
        QString subdirName = outputSubdirTextBox->text().trimmed();
        if (subdirName.isEmpty()) {
            subdirName = "yt-dlp output";
        }
        finalOutputDir = QDir(outputDir).filePath(subdirName);
    }
    QDir dir(finalOutputDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            consoleTextEdit->append("Error: Couldn't create the base output directory!");
            statusBar->showMessage("Error: Couldn't create the output directory!", 5000);
            return;
        }
    }
    if (!QFileInfo(finalOutputDir).isWritable()) {
        consoleTextEdit->append("Error: You don't have permission to write to this directory!");
        statusBar->showMessage("Error: No write permission for the output directory!", 5000);
        return;
    }
    consoleTextEdit->append("Downloads will be saved to: " + finalOutputDir);
    statusBar->showMessage("Starting download to " + finalOutputDir, 5000);
    downloadButton->setEnabled(false);
    cancelButton->setVisible(true);
    progressBar->setValue(0);
    progressBar->setVisible(true);
    consoleTextEdit->append("Starting download...");
    downloadStartTime = QDateTime::currentDateTime();
    filesToDelete.clear();

    QStringList channelPathRegexList;
    channelPathRegexList << "^(https?://(?:(?:www|m)\\.)?youtube\\.(?:com|co\\.[a-zA-Z]{2})/(?:(?:c/|channel/|user/|@[a-zA-Z0-9_.-]+)(?:/|$)))";
    QRegularExpression channelPathRegex(channelPathRegexList.join('|'), QRegularExpression::CaseInsensitiveOption);
    QRegularExpression playlistRegex(
        "^https?://(?:(?:www|m)\\.)?youtube\\.(?:com|co\\.[a-zA-Z]{2})/(?:playlist\\?list=[a-zA-Z0-9_.-]+|watch\\?v=[a-zA-Z0-9_.-]+&list=[a-zA-Z0-9_.-]+.*)$",
                                     QRegularExpression::CaseInsensitiveOption
    );
    bool isChannelUrl = channelPathRegex.match(urlTextBox->text().trimmed()).hasMatch();
    bool isPlaylistUrl = playlistRegex.match(urlTextBox->text().trimmed()).hasMatch();
    if (useSelectedItemsCheck->isChecked() && isPlaylistUrl) {
        QList<int> selectedIndices;
        for (int i = 0; i < playlistListWidget->count(); ++i) {
            QListWidgetItem *item = playlistListWidget->item(i);
            if (item->flags() & Qt::ItemIsUserCheckable && item->checkState() == Qt::Checked) {
                int originalIndex = item->data(OriginalIndexRole).toInt();
                if (originalIndex > 0) {
                    selectedIndices << originalIndex;
                }
            }
        }
        if (selectedIndices.isEmpty()) {
            consoleTextEdit->append("Error: No videos selected for download.");
            statusBar->showMessage("Error: No videos selected for download.", 5000);
            downloadButton->setEnabled(true);
            cancelButton->setVisible(false);
            progressBar->setVisible(false);
            return;
        }
        startDownload();
    } else if (useSelectedChannelItemsCheck->isChecked() && isChannelUrl) {
        QList<int> selectedIndices;
        for (int i = 0; i < channelListWidget->count(); ++i) {
            QListWidgetItem *item = channelListWidget->item(i);
            if (item->flags() & Qt::ItemIsUserCheckable && item->checkState() == Qt::Checked) {
                int originalIndex = item->data(OriginalIndexRole).toInt();
                if (originalIndex > 0) {
                    selectedIndices << originalIndex;
                }
            }
        }
        if (selectedIndices.isEmpty()) {
            consoleTextEdit->append("Error: No videos selected for download.");
            statusBar->showMessage("Error: No videos selected for download.", 5000);
            downloadButton->setEnabled(true);
            cancelButton->setVisible(false);
            progressBar->setVisible(false);
            return;
        }
        QString endpoint;
        if (channelContentComboBox->currentText() == "Videos") {
            endpoint = "/videos";
        } else if (channelContentComboBox->currentText() == "Shorts") {
            endpoint = "/shorts";
        } else if (channelContentComboBox->currentText() == "Live Streams") {
            endpoint = "/streams";
        }
        QString channelUrl = channelPathRegex.match(urlTextBox->text().trimmed()).captured(1);
        if (channelUrl.endsWith('/')) {
            channelUrl = channelUrl.left(channelUrl.length() - 1);
        }
        getSelectedChannelVideoUrls(channelUrl, endpoint, selectedIndices);
    } else {
        startDownload();
    }
}

void MainWindow::onCancelClicked() {
    if (process) {
        process->kill();
        QTimer::singleShot(1000, this, &MainWindow::onCancelCleanup);
    }
}

void MainWindow::onCancelCleanup() {
    for (const QString &file : filesToDelete) {
        QFile::remove(file);
        consoleTextEdit->append("Deleted file: " + file);
    }
    QString outputDir = outputDirTextBox->text();
    if (useOutputSubdirCheck->isChecked()) {
        QString subdirName = outputSubdirTextBox->text().trimmed();
        if (subdirName.isEmpty()) {
            subdirName = "yt-dlp output";
        }
        outputDir = QDir(outputDir).filePath(subdirName);
    }
    QDir dir(outputDir);
    QStringList partFiles = dir.entryList(QStringList() << "*.part", QDir::Files);
    for (const QString &partFile : partFiles) {
        if (QFileInfo(dir.filePath(partFile)).birthTime() > downloadStartTime) {
            QFile::remove(dir.filePath(partFile));
            consoleTextEdit->append("Deleted file: " + partFile);
        }
    }
    consoleTextEdit->append("Download cancelled.");
    statusBar->showMessage("Download cancelled", 5000);
    downloadButton->setEnabled(true);
    cancelButton->setVisible(false);
    progressBar->setVisible(false);
    downloadState = DownloadState::Idle;
}

void MainWindow::onProcessOutput() {
    QByteArray output = process->readAllStandardOutput();
    QString outputStr = QString::fromUtf8(output);
    consoleTextEdit->append(outputStr);

    // Track downloaded files
    if (outputStr.contains("[download] Destination:")) {
        QString filePath = outputStr.split("[download] Destination:").last().trimmed();
        filesToDelete.append(filePath);
        downloadState = DownloadState::Downloading;
    } else if (outputStr.contains("[Merger] Merging formats into")) {
        QString filePath = outputStr.split("[Merger] Merging formats into").last().trimmed().remove('"');
        filesToDelete.append(filePath);
        downloadState = DownloadState::PostProcessing;
    } else if (outputStr.contains("[sponsorblock]") || outputStr.contains("[ffmpeg]") || outputStr.contains("[Metadata]")) {
        downloadState = DownloadState::PostProcessing;
    }

    // Update progress bar
    if (downloadState == DownloadState::Downloading) {
        QRegularExpression ytDlpRegex("\\[download\\]\\s*(\\d+\\.\\d+)%");
        QRegularExpressionMatch ytDlpMatch = ytDlpRegex.match(outputStr);
        if (ytDlpMatch.hasMatch()) {
            double percentage = ytDlpMatch.captured(1).toDouble();
            progressBar->setValue(static_cast<int>(percentage));
        } else {
            QRegularExpression aria2cRegex("\\((\\d+)%\\)");
            QRegularExpressionMatch aria2cMatch = aria2cRegex.match(outputStr);
            if (aria2cMatch.hasMatch()) {
                double percentage = aria2cMatch.captured(1).toDouble();
                progressBar->setValue(static_cast<int>(percentage));
            }
        }
    } else if (downloadState == DownloadState::PostProcessing) {
        progressBar->setValue(100);
        progressBar->setFormat("Post-processing...");
    }
}

void MainWindow::onUseCookiesFileToggled(bool checked) {
    cookiesFileTextBox->setEnabled(checked);
    browseCookiesFileButton->setEnabled(checked);
    if (checked) {
        extractCookiesFromBrowserCheck->setChecked(false);
    }
    updateCommandPreview();
}

void MainWindow::onExtractCookiesFromBrowserToggled(bool checked) {
    browserComboBox->setEnabled(checked);
    browserProfileTextBox->setEnabled(checked);
    if (checked) {
        useCookiesFileCheck->setChecked(false);
    }
    updateCommandPreview();
}

void MainWindow::onBrowseCookiesFileClicked() {
    QString filePath = QFileDialog::getOpenFileName(this, "Select Cookies File", QDir::homePath(), "Cookies Files (*.txt *.cookie)");
    if (!filePath.isEmpty()) {
        cookiesFileTextBox->setText(filePath);
        updateCommandPreview();
    }
}

void MainWindow::updateCommandPreview() {
    QStringList args = buildCommand();
    QString command = "yt-dlp " + args.join(" ");
    commandPreviewTextBox->setText(command);
}

QStringList MainWindow::buildCommand() {
    QStringList args;
    args << "--progress";

    // General Options
    if (disableConfigCheck->isChecked()) {
        args << "--ignore-config";
    }
    if (continueOnErrorsCheck->isChecked()) {
        args << "--ignore-errors";
    }
    if (forceGenericExtractorCheck->isChecked()) {
        args << "--force-generic-extractor";
    }
    if (legacyServerConnectCheck->isChecked()) {
        args << "--legacy-server-connect";
    }
    if (ignoreSslCertificateCheck->isChecked()) {
        args << "--no-check-certificate";
    }
    if (embedThumbnailCheck->isChecked()) {
        args << "--embed-thumbnail";
    }
    if (addMetadataCheck->isChecked()) {
        args << "--add-metadata";
    }
    if (embedInfoJsonCheck->isChecked()) {
        args << "--embed-info-json";
    }
    if (embedChaptersCheck->isChecked()) {
        args << "--embed-chapters";
    }
    if (sleepIntervalCheck->isChecked()) {
        args << "--sleep-interval" << QString::number(sleepIntervalSlider->value());
    }
    if (waitForStreamCheck->isChecked()) {
        args << "--wait-for-video" << QString::number(waitForStreamSlider->value());
    }
    if (impersonateCheck->isChecked()) {
        args << "--impersonate" << impersonateComboBox->currentText();
    }
    // Playlist or Channel Selected Items
    QStringList urlsToDownload = urlTextBox->text().split(' ', Qt::SkipEmptyParts);
    if (urlsToDownload.isEmpty() && downloadState == DownloadState::Downloading) {
        consoleTextEdit->append("Error: No URLs provided for download.");
        return QStringList();
    }
    if (useSelectedItemsCheck->isChecked()) {
        QRegularExpression playlistRegex(
            "^https?://(?:(?:www|m)\\.)?youtube\\.(?:com|co\\.[a-zA-Z]{2})/(?:playlist\\?list=[a-zA-Z0-9_.-]+|watch\\?v=[a-zA-Z0-9_.-]+&list=[a-zA-Z0-9_.-]+.*)$",
                                         QRegularExpression::CaseInsensitiveOption
        );
        if (!urlsToDownload.isEmpty() && playlistRegex.match(urlsToDownload.first()).hasMatch()) {
            QList<int> selectedIndices;
            for (int i = 0; i < playlistListWidget->count(); ++i) {
                QListWidgetItem *item = playlistListWidget->item(i);
                if (item->flags() & Qt::ItemIsUserCheckable && item->checkState() == Qt::Checked) {
                    int originalIndex = item->data(OriginalIndexRole).toInt();
                    if (originalIndex > 0) {
                        selectedIndices << originalIndex;
                    }
                }
            }
            if (!selectedIndices.isEmpty()) {
                QString playlistItems = generatePlaylistItems(selectedIndices);
                args << "--playlist-items" << playlistItems;
            }
        }
    }
    // Subtitles Options
    if (downloadSubsCheck->isChecked()) {
        args << "--write-subs";
        if (allSubsRadio->isChecked()) {
            args << "--sub-langs" << "all";
        } else if (specificSubsRadio->isChecked()) {
            QStringList selectedLangs;
            for (int i = 0; i < subsLangList->count(); ++i) {
                QListWidgetItem *item = subsLangList->item(i);
                if (item->checkState() == Qt::Checked) {
                    selectedLangs << item->text();
                }
            }
            if (!selectedLangs.isEmpty()) {
                args << "--sub-langs" << selectedLangs.join(",");
            }
        }
        if (embedSubsCheck->isChecked()) {
            args << "--embed-subs";
        }
    }
    // SponsorBlock Options
    if (enableSponsorBlockCheck->isChecked()) {
        args << "--sponsorblock-mark" << "all";
        QStringList removeCategories;
        if (sponsorCheck->isChecked()) removeCategories << "sponsor";
        if (selfPromoCheck->isChecked()) removeCategories << "selfpromo";
        if (interactionCheck->isChecked()) removeCategories << "interaction";
        if (fillerCheck->isChecked()) removeCategories << "filler";
        if (outroCheck->isChecked()) removeCategories << "outro";
        if (introCheck->isChecked()) removeCategories << "intro";
        if (previewCheck->isChecked()) removeCategories << "preview";
        if (musicOfftopicCheck->isChecked()) removeCategories << "music_offtopic";
        if (!removeCategories.isEmpty()) {
            args << "--sponsorblock-remove" << removeCategories.join(",");
        }
    }
    // Filename Formatting Options
    if (autoNumberCheck->isChecked()) {
        args << "--autonumber-start" << "1";
    }
    if (restrictFilenamesCheck->isChecked()) {
        args << "--restrict-filenames";
    }
    if (replaceSpacesCheck->isChecked()) {
        args << "--replace-in-metadata" << "title,uploader,channel" << "\\s" << "_";
    }
    if (allowUnsafeExtCheck->isChecked()) {
        args << "--compat-options" << "allow-unsafe-ext";
    }
    if (allowOverwritesCheck->isChecked()) {
        args << "--force-overwrites";
    }
    QString customFormat = customFormatTextBox->text().trimmed();
    if (customFormat.isEmpty()) {
        customFormat = "%(title)s";
    }
    QString extension = extensionComboBox->currentText();
    if (extension == "Default") {
        extension = "%(ext)s";
    } else {
        args << "--merge-output-format" << extension;
    }
    QString customDirFormat = customOutputDirFormatTextBox->text().trimmed();
    if (customDirFormat.isEmpty()) {
        customDirFormat = outputDirFormatComboBox->currentText() == "Default" ? "" : outputDirFormatComboBox->currentText();
    }
    QString outputTemplate;
    if (useOutputSubdirCheck->isChecked()) {
        QString subdirName = outputSubdirTextBox->text().trimmed();
        if (subdirName.isEmpty()) {
            subdirName = "yt-dlp output";
        }
        if (customDirFormat.isEmpty()) {
            outputTemplate = QDir(outputDirTextBox->text()).filePath(
                subdirName + QDir::separator() + customFormat + "." + extension
            );
        } else {
            outputTemplate = QDir(outputDirTextBox->text()).filePath(
                subdirName + QDir::separator() +
                customDirFormat + QDir::separator() + customFormat + "." + extension
            );
        }
    } else {
        if (customDirFormat.isEmpty()) {
            outputTemplate = QDir(outputDirTextBox->text()).filePath(
                customFormat + "." + extension
            );
        } else {
            outputTemplate = QDir(outputDirTextBox->text()).filePath(
                customDirFormat + QDir::separator() + customFormat + "." + extension
            );
        }
    }
    args << "--output" << outputTemplate;
    if (trimFilenamesCheck->isChecked()) {
        args << "--trim-filenames" << QString::number(trimLengthSlider->value());
    }
    // Authorization Options
    if (enableAuthCheck->isChecked() && !usernameTextBox->text().isEmpty()) {
        args << "--username" << usernameTextBox->text();
    }
    if (enableAuthCheck->isChecked() && !passwordTextBox->text().isEmpty()) {
        args << "--password" << passwordTextBox->text();
    }
    // Additional urls from metadata
    if (downloadAdditionalUrlsCheck->isChecked() && !additionalUrlsRegexTextBox->text().isEmpty()) {
        args << "--parse-metadata" << additionalUrlsRegexTextBox->text();
    }
    // Cookie Options
    if (useCookiesFileCheck->isChecked() && !cookiesFileTextBox->text().isEmpty()) {
        args << "--cookies" << cookiesFileTextBox->text();
    } else if (extractCookiesFromBrowserCheck->isChecked()) {
        QString browser = browserComboBox->currentText().toLower();
        QString profile = browserProfileTextBox->text().trimmed();
        if (!profile.isEmpty()) {
            browser += ":" + profile;
        }
        args << "--cookies-from-browser" << browser;
    }
    QString selectedSection = downloadSectionsComboBox->currentText();
    if (selectedSection != "Disable" && selectedSection != "Download entire video") {
        QString sectionArg;
        if (selectedSection == "Download intro chapter") {
            sectionArg = "intro";
        } else if (selectedSection == "Download first 5 minutes") {
            sectionArg = "*0:00-5:00";
        } else if (selectedSection == "Download last 5 minutes") {
            sectionArg = "*-5:00-inf";
        } else if (selectedSection == "Custom chapter regex") {
            sectionArg = downloadSectionsTextBox->text().trimmed();
        } else if (selectedSection == "Custom time range") {
            QString timeRange = downloadSectionsTextBox->text().trimmed();
            if (!timeRange.isEmpty()) {
                sectionArg = "*" + timeRange;
            }
        }
        if (!sectionArg.isEmpty()) {
            args << "--download-sections" << sectionArg;
        }
    }
    QString selectedFormatCode = selectedFormatCodeTextBox->text().trimmed();
    if (!selectedFormatCode.isEmpty()) {
        args << "-f" << selectedFormatCode;
    }
    if (useAria2cCheck->isChecked()) {
        int index = aria2cOptionsCombo->currentIndex();
        if (index >= 0) {
            QStringList aria2cArgsList = {
                "-x 2 -s 2 -k 1M",
                "-x 4 -s 4 -k 2M",
                "-x 8 -s 8 -k 4M",
                "-x 16 -s 16 -k 8M"
            };
            args << "--external-downloader" << "aria2c";
            args << "--external-downloader-args" << aria2cArgsList[index];
        }
    }
    if (audioOnlyCheck->isChecked()) {
        args << "-x";
        args << "--audio-format" << audioFormatCombo->currentText();
    }
    if (!urlsToDownload.isEmpty()) {
        args += urlsToDownload;
    }
    return args;
}

void MainWindow::onSaveConfigClicked() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (configDir.isEmpty()) {
        consoleTextEdit->append("Error: Couldn't determine the config directory.");
        statusBar->showMessage("Error: Couldn't determine the config directory.", 5000);
        return;
    }
    QString ytDlpDir = configDir + "/yt-dlp";
    QDir dir(ytDlpDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            consoleTextEdit->append("Error: Couldn't create the yt-dlp config directory!");
            statusBar->showMessage("Error: Couldn't create the yt-dlp config directory!", 5000);
            return;
        }
    }
    QString configFilePath = ytDlpDir + "/config";
    QFile configFile(configFilePath);
    if (!configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        consoleTextEdit->append("Error: Couldn't open the config file for writing!");
        statusBar->showMessage("Error: Couldn't open the config file for writing!", 5000);
        return;
    }
    QTextStream out(&configFile);
    QStringList args = buildCommand();
    args.removeAll("--progress");
    args.removeAll("--ignore-config");
    for (const QString &url : urlTextBox->text().split(' ', Qt::SkipEmptyParts)) {
        args.removeAll(url);
    }
    for (int i = 0; i < args.size(); ) {
        if (args[i].startsWith("-")) {
            QString line = args[i];
            i++;
            while (i < args.size() && !args[i].startsWith("-")) {
                line += " " + args[i];
                i++;
            }
            out << line << "\n";
        } else {
            i++;
        }
    }
    configFile.close();
    consoleTextEdit->append("Configuration saved to " + configFilePath);
    statusBar->showMessage("Configuration saved successfully", 5000);
}

void MainWindow::onListFormatsClicked() {
    QString url = urlTextBox->text().trimmed();
    if (url.isEmpty()) {
        formatsTextEdit->append("Please enter a URL");
        return;
    }
    QStringList urls = url.split(' ', Qt::SkipEmptyParts);
    QString firstUrl = urls.first();
    formatsTextEdit->clear();
    formatsTextEdit->append("Listing formats for: " + firstUrl);
    listFormatsButton->setEnabled(false);
    QProcess *listProcess = new QProcess(this);
    connect(listProcess, &QProcess::readyReadStandardOutput, [this, listProcess]() {
        QByteArray output = listProcess->readAllStandardOutput();
        QString outputStr = QString::fromUtf8(output).trimmed();
        if (!outputStr.isEmpty()) {
            formatsTextEdit->append(outputStr);
        }
    });
    connect(listProcess, &QProcess::readyReadStandardError, [this, listProcess]() {
        QByteArray error = listProcess->readAllStandardError();
        QString errorStr = QString::fromUtf8(error).trimmed();
        if (!errorStr.isEmpty()) {
            formatsTextEdit->append("Error: " + errorStr);
        }
    });
    connect(listProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [this, listProcess](int exitCode, QProcess::ExitStatus exitStatus) {
        listFormatsButton->setEnabled(true);
        if (exitStatus == QProcess::CrashExit) {
            formatsTextEdit->append("Format listing process crashed.");
        } else if (exitCode != 0) {
            formatsTextEdit->append("Format listing failed with exit code " + QString::number(exitCode));
        } else {
            formatsTextEdit->append("Finished listing formats.");
        }
        listProcess->deleteLater();
    });
    listProcess->start("yt-dlp", QStringList() << "-F" << firstUrl);
}

void MainWindow::startDownload() {
    QStringList args = buildCommand();
    if (args.isEmpty()) {
        consoleTextEdit->append("Error: No valid download command generated.");
        statusBar->showMessage("Error: No valid download command.", 5000);
        downloadButton->setEnabled(true);
        cancelButton->setVisible(false);
        progressBar->setVisible(false);
        downloadState = DownloadState::Idle;
        emit downloadFinished();
        return;
    }
    process = new QProcess(this);
    downloadState = DownloadState::Downloading;
    progressBar->setFormat("%p%");
    connect(process, &QProcess::readyReadStandardOutput, this, &MainWindow::onProcessOutput);
    connect(process, &QProcess::errorOccurred, [this](QProcess::ProcessError /*error*/) {
        consoleTextEdit->append("Process error: " + process->errorString());
        statusBar->showMessage("Process error: " + process->errorString(), 5000);
        downloadButton->setEnabled(true);
        cancelButton->setVisible(false);
        progressBar->setVisible(false);
        downloadState = DownloadState::Idle;
        emit downloadFinished();
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [this](int, QProcess::ExitStatus) {
        consoleTextEdit->append("Download finished.");
        statusBar->showMessage("Download finished", 5000);
        downloadButton->setEnabled(true);
        cancelButton->setVisible(false);
        progressBar->setVisible(false);
        downloadState = DownloadState::Idle;
        emit downloadFinished();
    });
    process->start("yt-dlp", args);
}

void MainWindow::onListPlaylistClicked() {
    QString url = urlTextBox->text().trimmed();
    if (url.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem("Please enter a playlist URL");
        item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
        item->setForeground(Qt::red);
        playlistListWidget->addItem(item);
        QMessageBox::warning(this, "Empty URL", "Please enter a playlist URL.");
        return;
    }
    playlistSearchTextBox->clear();

    QRegularExpression playlistRegex(
        "^https?://(?:(?:www|m)\\.)?youtube\\.(?:com|co\\.[a-zA-Z]{2})/(?:playlist\\?list=[a-zA-Z0-9_.-]+|watch\\?v=[a-zA-Z0-9_.-]+&list=[a-zA-Z0-9_.-]+.*)$",
                                     QRegularExpression::CaseInsensitiveOption
    );
    QRegularExpression channelRegex(
        "^https?://(?:(?:www|m)\\.)?youtube\\.(?:com|co\\.[a-zA-Z]{2})/(?:(?:c/|channel/|user/|@[a-zA-Z0-9_.-]+)(?:/|$))",
                                    QRegularExpression::CaseInsensitiveOption
    );
    if (channelRegex.match(url).hasMatch()) {
        QListWidgetItem *item = new QListWidgetItem("Invalid URL: Channel URLs should be used in the 'Channel Browser' tab");
        item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
        item->setForeground(Qt::red);
        playlistListWidget->addItem(item);
        QMessageBox::warning(this, "Invalid URL",
                             "The entered URL is a channel URL. Please use the 'Channel Browser' tab for channel URLs.\n\n"
                             "Valid playlist examples:\n"
                             "- https://www.youtube.com/playlist?list=PL1234567890\n"
                             "- https://www.youtube.com/watch?v=XXYlFuWEuKI&list=RDQMgEzdN5RuCXE\n\n"
                             "For channel URLs like https://www.youtube.com/@ChannelName/videos, use the 'Channel Browser' tab.");
        return;
    }
    if (!playlistRegex.match(url).hasMatch()) {
        QListWidgetItem *item = new QListWidgetItem("Invalid URL: Please enter a valid YouTube playlist URL");
        item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
        item->setForeground(Qt::red);
        playlistListWidget->addItem(item);
        QMessageBox::warning(this, "Invalid URL",
                             "The entered URL is not a valid YouTube playlist URL.\n\n"
                             "Valid examples:\n"
                             "- https://www.youtube.com/playlist?list=PL1234567890\n"
                             "- https://www.youtube.com/watch?v=XXYlFuWEuKI&list=RDQMgEzdN5RuCXE");
        return;
    }

    playlistOutput.clear();
    playlistListWidget->clear();
    QListWidgetItem *startItem = new QListWidgetItem("Listing videos...");
    startItem->setFlags(startItem->flags() & ~Qt::ItemIsUserCheckable);
    playlistListWidget->addItem(startItem);
    listPlaylistButton->setEnabled(false);

    QProcess *listProcess = new QProcess(this);

    connect(listProcess, &QProcess::readyReadStandardOutput, [this, listProcess]() {
        playlistOutput += QString::fromUtf8(listProcess->readAllStandardOutput());
    });

    connect(listProcess, &QProcess::readyReadStandardError, [this, listProcess]() {
        QByteArray error = listProcess->readAllStandardError();
        QString errorStr = QString::fromUtf8(error).trimmed();
        if (!errorStr.isEmpty()) {
            QListWidgetItem *item = new QListWidgetItem("Error: " + errorStr);
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setForeground(Qt::red);
            playlistListWidget->addItem(item);
        }
    });

    // Process the output when finished
    connect(listProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [this, listProcess](int exitCode, QProcess::ExitStatus exitStatus) {
        listPlaylistButton->setEnabled(true);
        if (exitStatus == QProcess::CrashExit) {
            QListWidgetItem *item = new QListWidgetItem("Video listing process crashed.");
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setForeground(Qt::red);
            playlistListWidget->addItem(item);
        } else if (exitCode != 0) {
            QListWidgetItem *item = new QListWidgetItem("Video listing failed with exit code " + QString::number(exitCode));
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setForeground(Qt::red);
            playlistListWidget->addItem(item);
        } else {
            originalPlaylistTitles.clear();
            QStringList titles = playlistOutput.split('\n', Qt::SkipEmptyParts);
            for (const QString& title : titles) {
                QString trimmedTitle = title.trimmed();
                if (!trimmedTitle.isEmpty()) {
                    originalPlaylistTitles.append(trimmedTitle);
                }
            }
            playlistListWidget->clear();
            for (int i = 0; i < originalPlaylistTitles.size(); ++i) {
                const QString& title = originalPlaylistTitles[i];
                QListWidgetItem *item = new QListWidgetItem(title);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Unchecked);
                item->setData(OriginalIndexRole, i + 1);
                playlistListWidget->addItem(item);
            }
            QListWidgetItem *item = new QListWidgetItem("Finished listing videos.");
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setForeground(Qt::gray);
            playlistListWidget->addItem(item);
        }
        listProcess->deleteLater();
    });

    QStringList args = {"--get-title", "--flat-playlist", url};
    listProcess->start("yt-dlp", args);
}

void MainWindow::onListChannelClicked() {
    QString url = urlTextBox->text().trimmed();
    if (url.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem("Please enter a channel URL");
        item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
        item->setForeground(Qt::red);
        channelListWidget->addItem(item);
        QMessageBox::warning(this, "Empty URL", "Please enter a channel URL.");
        return;
    }
    channelSearchTextBox->clear();
    QRegularExpression channelPathRegex(
        "^(https?://(?:(?:www|m)\\.)?youtube\\.(?:com|co\\.[a-zA-Z]{2})/(?:(?:c/|channel/|user/|@[a-zA-Z0-9_.-]+)(?:/|$)))",
                                        QRegularExpression::CaseInsensitiveOption
    );
    QRegularExpressionMatch match = channelPathRegex.match(url);
    if (!match.hasMatch()) {
        QListWidgetItem *item = new QListWidgetItem("Invalid URL: Please enter a valid YouTube channel URL");
        item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
        item->setForeground(Qt::red);
        channelListWidget->addItem(item);
        QMessageBox::warning(this, "Invalid URL",
                             "The entered URL is not a valid YouTube channel URL.\n\n"
                             "Valid examples (extra paths like /videos or /featured are ignored):\n"
                             "- https://www.youtube.com/@ChannelName\n"
                             "- https://www.youtube.com/@ChannelName/featured\n"
                             "- https://www.youtube.com/@ChannelName/videos\n"
                             "- https://www.youtube.com/channel/UC1234567890\n"
                             "- https://www.youtube.com/c/ChannelName\n"
                             "- https://www.youtube.com/user/Username\n"
                             "- https://m.youtube.com/@ChannelName\n"
                             "- https://www.youtube.co.uk/@ChannelName/playlists\n\n"
                             "Invalid examples:\n"
                             "- https://www.youtube.com/watch?v=XXYlFuWEuKI");
        return;
    }

    QString normalizedUrl = match.captured(1);
    if (normalizedUrl.endsWith('/')) {
        normalizedUrl = normalizedUrl.left(normalizedUrl.length() - 1);
    }

    QString contentType = channelContentComboBox->currentText();
    QString endpoint;
    if (contentType == "Videos") {
        endpoint = "/videos";
    } else if (contentType == "Shorts") {
        endpoint = "/shorts";
    } else if (contentType == "Live Streams") {
        endpoint = "/streams";
    }
    QString fullUrl = normalizedUrl + endpoint;

    channelOutput.clear();
    channelListWidget->clear();
    videoCountLabel->setText("0 listed");
    QString startMsg = "Listing " + contentType.toLower() + "...";
    if (showUploadDatesCheck->isChecked()) {
        startMsg += " (may take time for large lists)";
    }
    QListWidgetItem *startItem = new QListWidgetItem(startMsg);
    startItem->setFlags(startItem->flags() & ~Qt::ItemIsUserCheckable);
    channelListWidget->addItem(startItem);
    listChannelButton->setEnabled(false);

    QProcess *listProcess = new QProcess(this);

    connect(listProcess, &QProcess::readyReadStandardOutput, [this, listProcess]() {
        channelOutput += QString::fromUtf8(listProcess->readAllStandardOutput());
    });

    connect(listProcess, &QProcess::readyReadStandardError, [this, listProcess]() {
        QByteArray error = listProcess->readAllStandardError();
        QString errorStr = QString::fromUtf8(error).trimmed();
        if (!errorStr.isEmpty()) {
            QListWidgetItem *item = new QListWidgetItem("Error: " + errorStr);
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setForeground(Qt::red);
            channelListWidget->addItem(item);
            consoleTextEdit->append("Error: " + errorStr);
            statusBar->showMessage("Error listing videos.", 5000);
        }
    });

    // Process the output when finished
    connect(listProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [this, listProcess, contentType, fullUrl](int exitCode, QProcess::ExitStatus exitStatus) {
        listChannelButton->setEnabled(true);
        if (exitStatus == QProcess::CrashExit) {
            QListWidgetItem *item = new QListWidgetItem(contentType + " listing process crashed.");
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setForeground(Qt::red);
            channelListWidget->addItem(item);
            consoleTextEdit->append(contentType + " listing process crashed for " + fullUrl);
            statusBar->showMessage(contentType + " listing process crashed.", 5000);
        } else if (exitCode != 0) {
            QListWidgetItem *item = new QListWidgetItem(contentType + " listing failed with exit code " + QString::number(exitCode));
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setForeground(Qt::red);
            channelListWidget->addItem(item);
            consoleTextEdit->append(contentType + " listing failed with exit code " + QString::number(exitCode));
            statusBar->showMessage(contentType + " listing failed.", 5000);
        } else {
            originalChannelData.clear();
            QStringList lines = channelOutput.split('\n', Qt::SkipEmptyParts);
            channelListWidget->clear();
            bool showDates = showUploadDatesCheck->isChecked();
            for (int i = 0; i < lines.size(); ++i) {
                QString line = lines[i].trimmed();
                if (line.isEmpty()) continue;
                QString title;
                QString formattedDate = "";
                if (showDates) {
                    QStringList parts = line.split('\t');
                    title = parts.value(0).trimmed();
                    QString rawDate = parts.value(1).trimmed();
                    if (!rawDate.isEmpty() && rawDate != "NA" && rawDate.length() == 8) {
                        formattedDate = rawDate.left(4) + "-" + rawDate.mid(4, 2) + "-" + rawDate.right(2);
                    }
                } else {
                    title = line;
                }
                QString displayText = title;
                if (!formattedDate.isEmpty()) {
                    displayText += " (" + formattedDate + ")";
                }
                originalChannelData.append(qMakePair(title, formattedDate));
                QListWidgetItem *item = new QListWidgetItem(displayText);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Unchecked);
                item->setData(OriginalIndexRole, i + 1);
                channelListWidget->addItem(item);
            }
            updateUseSelectedChannelCheck();
            QListWidgetItem *item = new QListWidgetItem("Finished listing " + contentType.toLower() + ".");
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setForeground(Qt::gray);
            channelListWidget->addItem(item);
            videoCountLabel->setText(QString("%1 listed").arg(originalChannelData.size()));
            consoleTextEdit->append(QString("Finished listing %1 (%2 items).").arg(contentType.toLower()).arg(originalChannelData.size()));
            statusBar->showMessage(QString("Listed %1 %2.").arg(originalChannelData.size()).arg(contentType.toLower()), 5000);
        }
        listProcess->deleteLater();
    });

    QStringList args;
    bool showDates = showUploadDatesCheck->isChecked();
    if (showDates) {
        args << "--print" << "%(title)s\t%(upload_date)s";
    } else {
        args << "--get-title" << "--flat-playlist";
    }
    QString limit = listLimitComboBox->currentText();
    if (limit != "All") {
        args << "--playlist-end" << limit;
    }
    args << fullUrl;
    listProcess->start("yt-dlp", args);
}

void MainWindow::onPlaylistSearchTextChanged(const QString &text) {
    playlistListWidget->clear();
    QString searchText = text.trimmed().toLower();

    if (searchText.isEmpty()) {
        for (int i = 0; i < originalPlaylistTitles.size(); ++i) {
            const QString& title = originalPlaylistTitles[i];
            QListWidgetItem *item = new QListWidgetItem(title);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            item->setData(OriginalIndexRole, i + 1);
            playlistListWidget->addItem(item);
        }
    } else {
        for (int i = 0; i < originalPlaylistTitles.size(); ++i) {
            const QString& title = originalPlaylistTitles[i];
            if (title.toLower().contains(searchText)) {
                QListWidgetItem *item = new QListWidgetItem(title);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Unchecked);
                item->setData(OriginalIndexRole, i + 1);
                playlistListWidget->addItem(item);
            }
        }
    }
}

void MainWindow::onChannelSearchTextChanged(const QString &text) {
    channelListWidget->clear();
    QString searchText = text.trimmed().toLower();
    bool showDates = showUploadDatesCheck->isChecked();

    for (int i = 0; i < originalChannelData.size(); ++i) {
        const QPair<QString, QString>& data = originalChannelData[i];
        const QString& title = data.first;
        const QString& formattedDate = data.second;
        if (searchText.isEmpty() || title.toLower().contains(searchText)) {
            QString displayText = title;
            if (showDates && !formattedDate.isEmpty()) {
                displayText += " (" + formattedDate + ")";
            }
            QListWidgetItem *item = new QListWidgetItem(displayText);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            item->setData(OriginalIndexRole, i + 1);
            channelListWidget->addItem(item);
        }
    }
    videoCountLabel->setText(QString("%1 listed").arg(channelListWidget->count()));
    updateUseSelectedChannelCheck();
}

QString MainWindow::generatePlaylistItems(const QList<int>& indices) {
    if (indices.isEmpty()) return "";
    QList<int> sortedIndices = indices;
    std::sort(sortedIndices.begin(), sortedIndices.end());
    QStringList items;
    int start = sortedIndices[0];
    int end = start;
    for (int i = 1; i < sortedIndices.size(); ++i) {
        if (sortedIndices[i] == end + 1) {
            end = sortedIndices[i];
        } else {
            if (start == end) {
                items << QString::number(start);
            } else {
                items << QString("%1-%2").arg(start).arg(end);
            }
            start = sortedIndices[i];
            end = start;
        }
    }
    if (start == end) {
        items << QString::number(start);
    } else {
        items << QString("%1-%2").arg(start).arg(end);
    }
    return items.join(",");
}

QStringList MainWindow::getSelectedChannelVideoUrls(const QString& channelUrl, const QString& endpoint, const QList<int>& indices) {
    QStringList videoUrls;
    if (indices.isEmpty()) {
        consoleTextEdit->append("Error: No indices selected for channel download.");
        statusBar->showMessage("Error: No indices selected for channel download.", 5000);
        emit downloadFinished();
        return videoUrls;
    }

    QString fullUrl = channelUrl + endpoint;
    this->channelUrl = channelUrl;
    this->endpoint = endpoint;
    this->indices = indices;
    this->fetchAttempt = 0;
    this->maxRetries = 3;
    this->channelOutput.clear();

    if (urlFetchProcess) {
        if (urlFetchProcess->state() == QProcess::Running) {
            urlFetchProcess->kill();
            urlFetchProcess->waitForFinished(1000);
        }
        delete urlFetchProcess;
        urlFetchProcess = nullptr;
    }

    urlFetchProcess = new QProcess(this);
    QStringList args = {"--get-url", "--flat-playlist", "--no-playlist", "--no-cache-dir"};
    QString limit = listLimitComboBox->currentText();
    if (limit != "All") {
        args << "--playlist-end" << limit;
    }
    args << fullUrl;

    connect(urlFetchProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        channelOutput += QString::fromUtf8(urlFetchProcess->readAllStandardOutput());
    });

    connect(urlFetchProcess, &QProcess::readyReadStandardError, this, [this]() {
        QByteArray error = urlFetchProcess->readAllStandardError();
        QString errorStr = QString::fromUtf8(error).trimmed();
        if (!errorStr.isEmpty()) {
            consoleTextEdit->append("Error retrieving video URLs: " + errorStr);
            statusBar->showMessage("Error retrieving video URLs.", 5000);
        }
    });

    connect(urlFetchProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &MainWindow::onUrlFetchFinished);

    urlFetchProcess->start("yt-dlp", args);
    if (!urlFetchProcess->waitForStarted(5000)) {
        consoleTextEdit->append("Error: Failed to start process for retrieving video URLs.");
        statusBar->showMessage("Error: Failed to start URL retrieval process.", 5000);
        emit downloadFinished();
        delete urlFetchProcess;
        urlFetchProcess = nullptr;
        return QStringList();
    }

    return QStringList();
}

void MainWindow::onUrlFetchFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    QStringList videoUrls;
    QString fullUrl = channelUrl + endpoint;

    if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        consoleTextEdit->append("Failed to retrieve video URLs (exit code: " + QString::number(exitCode) + ").");
        statusBar->showMessage("Failed to retrieve video URLs.", 5000);
        fetchAttempt++;
        if (fetchAttempt < maxRetries) {
            consoleTextEdit->append(QString("Retrying URL retrieval (attempt %1 of %2)...").arg(fetchAttempt + 1).arg(maxRetries));
            QThread::msleep(1000);
            if (urlFetchProcess) {
                QStringList retryArgs = {"--get-url", "--flat-playlist", "--no-playlist", "--no-cache-dir"};
                QString limit = listLimitComboBox->currentText();
                if (limit != "All") {
                    retryArgs << "--playlist-end" << limit;
                }
                retryArgs << fullUrl;
                urlFetchProcess->start("yt-dlp", retryArgs);
            } else {
                consoleTextEdit->append("Error: URL fetch process unavailable for retry.");
                statusBar->showMessage("Error: URL fetch process unavailable.", 5000);
                downloadButton->setEnabled(true);
                cancelButton->setVisible(false);
                progressBar->setVisible(false);
                downloadState = DownloadState::Idle;
                emit downloadFinished();
            }
            return;
        } else {
            consoleTextEdit->append("Error: No valid video URLs retrieved after " + QString::number(maxRetries) + " attempts.");
            statusBar->showMessage("Error: No valid video URLs retrieved.", 5000);
            downloadButton->setEnabled(true);
            cancelButton->setVisible(false);
            progressBar->setVisible(false);
            downloadState = DownloadState::Idle;
            emit downloadFinished();
            if (urlFetchProcess) {
                delete urlFetchProcess;
                urlFetchProcess = nullptr;
            }
            return;
        }
    }

    QStringList allUrls = channelOutput.split('\n', Qt::SkipEmptyParts);
    for (QString& url : allUrls) {
        url = url.trimmed();
    }

    for (int index : indices) {
        if (index > 0 && index <= allUrls.size()) {
            videoUrls << allUrls[index - 1];
            consoleTextEdit->append(QString("Selected URL %1: %2").arg(index).arg(allUrls[index - 1]));
        } else {
            consoleTextEdit->append(QString("Warning: Index %1 is out of range (max %2).").arg(index).arg(allUrls.size()));
            statusBar->showMessage(QString("Warning: Index %1 out of range.").arg(index), 5000);
        }
    }

    if (videoUrls.isEmpty()) {
        consoleTextEdit->append("Error: No valid video URLs retrieved for selected indices.");
        statusBar->showMessage("Error: No valid video URLs retrieved.", 5000);
        downloadButton->setEnabled(true);
        cancelButton->setVisible(false);
        progressBar->setVisible(false);
        downloadState = DownloadState::Idle;
        emit downloadFinished();
        if (urlFetchProcess) {
            delete urlFetchProcess;
            urlFetchProcess = nullptr;
        }
        return;
    }

    QString originalUrl = urlTextBox->text();
    urlTextBox->setText(videoUrls.join(" "));
    consoleTextEdit->append(QString("Initiating download for %1 item%2: %3")
    .arg(videoUrls.size())
    .arg(videoUrls.size() == 1 ? "" : "s")
    .arg(videoUrls.join(", ")));
    connect(this, &MainWindow::downloadFinished, this, [this, originalUrl]() {
        urlTextBox->setText(originalUrl);
    }, Qt::SingleShotConnection);

    if (urlFetchProcess) {
        delete urlFetchProcess;
        urlFetchProcess = nullptr;
    }
    channelOutput.clear();

    startDownload();
}

void MainWindow::updateUseSelectedChannelCheck() {
    bool hasChecked = false;
    for (int i = 0; i < channelListWidget->count(); ++i) {
        QListWidgetItem *item = channelListWidget->item(i);
        if (item->flags() & Qt::ItemIsUserCheckable && item->checkState() == Qt::Checked) {
            hasChecked = true;
            break;
        }
    }
    useSelectedChannelItemsCheck->setChecked(hasChecked);
}
