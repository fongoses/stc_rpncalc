// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <QDebug>
#include "calc_main.h"
#include "../src/lcd.h"
#include "calculator.h"

Calculator::Calculator(QObject *parent) :
	QObject(parent),
	lcd_thread(*this),
	m_lcdText("calculator initial text")
{
	qDebug() << "Starting lcd thread";
	lcd_thread.start();
	qDebug() << "lcd thread started";
	qDebug() << "Starting calculator thread";
	calc_thread.start();
	qDebug() << "calculator thread started";

	updateLcd();
}

Calculator::~Calculator(){
	quit();
	while (!calc_thread.isFinished()); //TODO: timeout
	while (!lcd_thread.isFinished()); //TODO: timeout
}

void Calculator::quit(){
	ExitCalcMain = 1;
	LcdAvailable.release();
	KeysAvailable.release();
	qDebug() << "quitting...";
}

void Calculator::buttonClicked(const QString& in) {
	QStringList split = in.split(",");
	int8_t row = split[0].toInt();
	int8_t col = split[1].toInt();
	//translate column from left indexed, to right indexed
	static const int NUM_COLS = 4;
	col = (NUM_COLS - 1) - col;
	//get keycode
	int8_t keycode = col + NUM_COLS*row;
//	qDebug() << "keycode: " << keycode;
//	qDebug() << " row: " << row << ", col: " << col;
	//push keycode
	#define INCR_NEW_KEY_I(i) i = (i + 1) & 3
	if (!NewKeyEmpty && (new_key_write_i == new_key_read_i)){
		printf("ERROR: key fifo full\n");
		return;
	}
	NewKeyBuf[new_key_write_i] = keycode;
	INCR_NEW_KEY_I(new_key_write_i);
	NewKeyEmpty = 0;
	KeysAvailable.release();
}

void Calculator::updateLcd() {
	QString tmp("lcd text:\n");
	const char* lcd_buf = get_lcd_buf();
	for (int i = 0; i < MAX_ROWS; i++){
		tmp += "|";
		for (int j = 0; j < MAX_CHARS_PER_LINE; j++){
			tmp += lcd_buf[j + i*MAX_CHARS_PER_LINE];
		}
		tmp += "|\n";
	}
//	qDebug() << "update lcd:" << tmp.toStdString().c_str();

	setLcdText(tmp);
}

void Calculator::setLcdText(const QString &lcdText){
	m_lcdText = lcdText;
	emit lcdTextChanged();
}


CalcLcdThread::CalcLcdThread(Calculator &calc) :
	m_calc(calc)
{

}

void CalcLcdThread::run(){
	while(1){
		if (ExitCalcMain){
			return;
		}
		LcdAvailable.acquire();
		m_calc.updateLcd();
	}
}


void CalcMainThread::run() {
	calc_main();
}

