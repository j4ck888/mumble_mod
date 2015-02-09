#include "LanguageChangeNotice.h"

LanguageChangeNotice::LanguageChangeNotice(QLabel *languageLabel, int startLanguageID) {
	this->languageLabel = languageLabel;
	this->startLanguageID = startLanguageID;
	this->originalLabelText = languageLabel->text();
	this->restartNotice = QString::fromUtf8(" <span style=\" color:#ff0000;\">(Mumble restart needed)</span>");
}

void LanguageChangeNotice::languageChanged(int newLanguageID) {
	if (newLanguageID == startLanguageID) {
		languageLabel->setText(originalLabelText);
	} else {
		languageLabel->setText(originalLabelText + restartNotice);
	}
}
