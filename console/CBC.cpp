/**************************************************************************
 *  Copyright 2007,2008,2009 KISS Institute for Practical Robotics        *
 *                                                                        *
 *  This file is part of KISS (Kipr's Instructional Software System).     *
 *                                                                        *
 *  KISS is free software: you can redistribute it and/or modify          *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation, either version 2 of the License, or     *
 *  (at your option) any later version.                                   *
 *                                                                        *
 *  KISS is distributed in the hope that it will be useful,               *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *  GNU General Public License for more details.                          *
 *                                                                        *
 *  You should have received a copy of the GNU General Public License     *
 *  along with KISS.  Check the LICENSE file in the project root.         *
 *  If not, see <http://www.gnu.org/licenses/>.                           *
 **************************************************************************/

#include "CBC.h"

#include <QProcess>
#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QRegExp>
#include <QtGlobal>
#include <QBuffer>
#include <QDateTime>
#include <QDebug>

CBC::CBC()
{
#ifdef Q_OS_WIN32
	m_gccPath = QDir::currentPath() + "/targets/gcc/mingw/bin/gcc.exe";
#else
	m_gccPath="/usr/bin/gcc";
#endif

	QFileInfo gccExecutable(m_gccPath);
	if(!gccExecutable.exists())
		qWarning() << "Error Could not find GCC Executable!";

	m_gcc.setReadChannel(QProcess::StandardError);

//FIXME This is ugly
#ifdef Q_OS_MAC
	system(("ranlib " + QDir::currentPath() + "/targets/gcc/lib/*.a").toLocal8Bit());
	system(("ranlib " + QDir::currentPath() + "/targets/cbc/lib/*.a").toLocal8Bit());
#endif
}

CBC::~CBC()
{
	m_gcc.kill();
	m_outputBinary.kill();
}

bool CBC::compile(QString filename)
{
	QFileInfo sourceInfo(filename);
	QStringList args;

	refreshSettings();

#ifdef Q_OS_WIN32
	m_outputFileName = sourceInfo.dir().absoluteFilePath(sourceInfo.baseName() + ".exe");
#else
	m_outputFileName = sourceInfo.dir().absoluteFilePath(sourceInfo.baseName());
#endif
	QString objectName = sourceInfo.dir().absoluteFilePath(sourceInfo.baseName() + ".o");

	QFileInfo outputInfo(m_outputFileName);
	if(sourceInfo.lastModified() < outputInfo.lastModified())
		return true;

	args = m_cflags;
	m_defaultPort.replace("\\", "\\\\");
	args << "-DDEFAULT_SERIAL_PORT=\"" + m_defaultPort + "\"";
	args << "-c" << filename << "-o" << objectName;
	m_gcc.start(m_gccPath, args);
	m_gcc.waitForFinished();
	processCompilerOutput();

	if(m_gcc.exitCode() != 0)
		return false;

	args.clear();
	args << "-o" << m_outputFileName << objectName;
	args << m_lflags;
	m_gcc.start(m_gccPath, args);
	m_gcc.waitForFinished();
	processLinkerOutput();

	QFile objectFile(objectName);
	objectFile.remove();

	if(m_gcc.exitCode() == 0)
		return true;
	return false;
}

QStringList CBC::getPaths(QString string)
{
    QStringList list;
    
    string.remove(QRegExp("^\\w*\\.o\\:\\s*"));
    string.replace(QRegExp("\\s*\\W\\r?\\n\\s*"), " ");
    string.remove("\n");
    string += " ";
    
    qWarning("string=\"%s\"", qPrintable(string));
    
    while(1) {
        int index = string.indexOf(QRegExp("\\w[ ]"));
        if(index == -1) return list;
        list << string.left(index+1).remove("\\");
        string.remove(0, index+1);
        while(!string.isEmpty() && string[0] == ' ') string.remove(0,1);
   }
}

bool CBC::download(QString filename)
{
    if(!compile(filename))
        return false;
    
    qWarning("Calling gcc...");
    m_gcc.reset();
    
    m_gcc.start(m_gccPath, QStringList() << "-E" << "-Wp,-MM" << filename);
    m_gcc.waitForFinished();
    
    qWarning("Gcc finished...");
    
    QString depString = QString::fromLocal8Bit(m_gcc.readAllStandardOutput());
    
    QStringList deps = getPaths(depString);
    
    deps.removeFirst();
    qWarning("deps.size()=%d", deps.size());
    
    qWarning("deps=\"%s\"", qPrintable(deps.join(",")));
    
    qWarning("Calling sendFile");
    
    if(!QSerialPort(m_defaultPort).open(QIODevice::ReadWrite)) {
        emit requestPort();
        if(!QSerialPort(m_defaultPort).open(QIODevice::ReadWrite))
            return false;
    }
    
    m_serial.setPort(m_defaultPort);
    return m_serial.sendFile(filename, deps);
}


void CBC::processCompilerOutput()
{
	bool foundError=false,foundWarning=false;

	while(m_gcc.canReadLine()) {
		QString inputLine = QString::fromLocal8Bit(m_gcc.readLine());
		QString outputLine;

		inputLine.remove(QRegExp("\\r*\\n$"));

                qWarning() << inputLine;

		inputLine.remove(QRegExp("^C\\:"));
		inputLine.remove(QRegExp("^/.*/(?=\\S*\\:)"));

		outputLine += inputLine.section(':',0,0) + ":";

		if(inputLine.section(':',2,2).length() > 0) {
			outputLine += inputLine.section(':',1,1) + ":";
			outputLine += inputLine.section(':',2,2).remove(' ') + ":";
			outputLine += inputLine.section(':', 3);
		}
		else
			outputLine += inputLine.section(':', 1);

		if(outputLine.section(':',2,2) == "error") {
                        qWarning() << outputLine;
			foundError=true;
		}
		else if(outputLine.section(':', 2,2) == "warning") {
                        qWarning() << outputLine;
			foundWarning=true;
		}
		else {
                        qWarning() << inputLine;
		}
	}
}

void CBC::processLinkerOutput()
{
	while(m_gcc.canReadLine()) {
		QString line = QString::fromLocal8Bit(m_gcc.readLine()).remove(QRegExp("\\r*\\n$"));
                qWarning() << line;
	}
}

void CBC::refreshSettings()
{
	QStringList include_dirs;
	QStringList lib_dirs;
	QSettings settings(m_targetFile, QSettings::IniFormat);

	m_cflags.clear();
	m_lflags.clear();

	include_dirs = settings.value("Target/include_dirs").toString().split(' ', QString::SkipEmptyParts);
	lib_dirs = settings.value("Target/lib_dirs").toString().split(' ', QString::SkipEmptyParts);

	QStringListIterator i(include_dirs);
	while(i.hasNext()) {
		QDir includePath(i.next());
		if(includePath.isAbsolute())
			m_cflags << "-I" + includePath.path();
		else
			m_cflags << "-I" + QDir::currentPath() + "/" + includePath.path();
	}

	i = lib_dirs;
	while(i.hasNext()) {
		QDir libPath(i.next());
		if(libPath.isAbsolute())
			m_lflags << "-L" + libPath.path();
		else
			m_lflags << "-L" + QDir::currentPath() + "/" + libPath.path();
	}

	m_cflags << settings.value("Target/cflags").toString().split(' ', QString::SkipEmptyParts);
	m_lflags << settings.value("Target/lflags").toString().split(' ', QString::SkipEmptyParts);
}
