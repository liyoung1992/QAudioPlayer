#ifndef QAUDIODIALOG_H
#define QAUDIODIALOG_H

#include <QDialog>
#include <QAudioDeviceInfo>

namespace Ui {
class QAudioDialog;
}

class QComboBox;
class AudioFileLoader;
class AudioBuffer;
class Player;
class QSliderButton;

class QAudioDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QAudioDialog(QWidget *parent = nullptr);
    ~QAudioDialog();

private slots:
    void on_pushButtonOpen_clicked();

    void on_pushButtonCloseFile_clicked();

    void on_pushButtonPlay_clicked(bool checked);

    void playerStarted();
    void playerStoped();
    void playerReadySlot(bool);
    void playerPositionChanged(qint32);

    void on_pushButtonLoop_clicked(bool checked);

    void on_horizontalSliderProgress_valueChanged(int value);

    void on_horizontalSliderProgress_sliderReleased();

    void on_pushButtonGoBegin_clicked();

    void on_pushButtonGoEnd_clicked();

    void on_comboBoxOutputDevice_currentIndexChanged(int index);

    void on_horizontalSliderProgress_sliderPressed();

protected:
    QSliderButton *audioVolume;
    QAudioDeviceInfo getSelectedDevice();
    bool  selectComboBoxItem(QComboBox * b, const QString & txt);
    QString audioFileSuffixLoad;
    QString workingDir;
    Player *player;
    void readSettings();
    void writeSettings();
    virtual void closeEvent(QCloseEvent *e);
private:
    Ui::QAudioDialog *ui;
};

#endif // QAUDIODIALOG_H
