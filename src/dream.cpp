/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright 2013 Joshua C. Klontz                                           *
 *                                                                           *
 * Licensed under the Apache License, Version 2.0 (the "License");           *
 * you may not use this file except in compliance with the License.          *
 * You may obtain a copy of the License at                                   *
 *                                                                           *
 *     http://www.apache.org/licenses/LICENSE-2.0                            *
 *                                                                           *
 * Unless required by applicable law or agreed to in writing, software       *
 * distributed under the License is distributed on an "AS IS" BASIS,         *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  *
 * See the License for the specific language governing permissions and       *
 * limitations under the License.                                            *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <cassert>
#include <string>
#include <QtCore>
#include <QtOpenGL>
#include <QtWidgets>
#include <llvm/Support/CommandLine.h>

/*!
 * \page integrated_development_environment Integrated Development Environment
 * \brief Documentation for the \c dream Integrated Development Environment (IDE).
 *
 * This application takes one optional parameter:
\verbatim
$ dream [<source_file>]
\endverbatim
 *
 * \c dream offers an interactive editor for Likely source code.
 * It is our playground for experimenting with [live coding](http://www.worrydream.com) ideas.
 */

/// \cond IDE
#include <likely.h>

using namespace llvm;
using namespace std;

static cl::opt<string> DreamInput(cl::Positional, cl::desc("<source_file>"), cl::init(""));
static cl::opt<bool> DreamSpartan("spartan", cl::desc("Hide the source code, only show the output"));
static cl::alias     DreamSpartanA("s", cl::desc("Alias for -spartan"), cl::aliasopt(DreamSpartan));

class CommandMode : public QObject
{
    Q_OBJECT
    CommandMode() = default; // It's a singleton

    bool eventFilter(QObject *obj, QEvent *event)
    {
        if ((event->type() == QEvent::KeyPress) ||
            (event->type() == QEvent::KeyRelease)) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Control) {
                if (event->type() == QEvent::KeyPress) {
                    if (!enabled) {
                        QGuiApplication::setOverrideCursor(Qt::PointingHandCursor);
                        enabled = true;
                        emit changed(enabled);
                    }
                } else {
                    if (enabled) {
                        QGuiApplication::restoreOverrideCursor();
                        enabled = false;
                        emit changed(enabled);
                    }
                }
            }
        }
        return QObject::eventFilter(obj, event);
    }

public:
    static bool enabled;
    static CommandMode *get()
    {
        static CommandMode *commandMode = new CommandMode();
        return commandMode;
    }

signals:
    void changed(bool);
};

bool CommandMode::enabled = false;

class SyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
    QTextCharFormat codeFormat, commentFormat, numberFormat, stringFormat;
    likely_env env = NULL;

public:
    SyntaxHighlighter(QTextDocument *parent)
        : QSyntaxHighlighter(parent)
    {
        commentFormat.setForeground(Qt::darkGray);
        numberFormat.setFontUnderline(true);
        numberFormat.setUnderlineStyle(QTextCharFormat::DotLine);
        stringFormat.setForeground(Qt::darkGreen);
        connect(CommandMode::get(), SIGNAL(changed(bool)), this, SLOT(rehighlight()));
    }

    ~SyntaxHighlighter()
    {
        likely_release_env(env);
    }

public slots:
    void highlight(likely_const_env env)
    {
        likely_release_env(this->env);
        this->env = likely_retain_env(env);
        for (uint32_t i=env->ast->begin_line; i<=env->ast->end_line; i++)
            rehighlightBlock(document()->findBlockByNumber(i));
    }

private:
    void highlightAST(uint32_t line, likely_const_ast ast)
    {
        if (ast->type == likely_ast_list) {
            for (uint32_t i=0; i<ast->num_atoms; i++)
                highlightAST(line, ast->atoms[i]);
        } else {
            assert(ast->begin_line == ast->end_line);
            if (ast->begin_line == line) {
                if (ast->type == likely_ast_string) {
                    setFormat(ast->begin_column, ast->end_column - ast->begin_column, stringFormat);
                } else if (CommandMode::enabled && ((ast->type == likely_ast_integer) || (ast->type == likely_ast_real))) {
                    setFormat(ast->begin_column, ast->end_column - ast->begin_column, numberFormat);
                }
            }
        }
    }

    void highlightBlock(const QString &text)
    {
        setFormat(0, text.size(), commentFormat); // Assume it's a comment until we prove otherwise
        const uint32_t line = uint32_t(currentBlock().blockNumber());
        likely_const_env it = env;
        likely_const_ast root_ast = getRoot(env ? env->ast : NULL);
        while (it && it->ast && (it->ast->end_line >= line) && (getRoot(it->ast) == root_ast)) {
            if ((line >= it->ast->begin_line) && (line <= it->ast->end_line)) {
                // It's code
                const uint32_t begin = (line == it->ast->begin_line) ? it->ast->begin_column : 0;
                const uint32_t end = (line == it->ast->end_line) ? it->ast->end_column : text.size();
                setFormat(begin, end-begin, codeFormat);
                highlightAST(line, it->ast);
            }
            it = it->parent;
        }
    }

    static likely_const_ast getRoot(likely_const_ast ast)
    {
        while (ast && ast->parent)
            ast = ast->parent;
        return ast;
    }
};

class Source : public QPlainTextEdit
{
    Q_OBJECT
    QString header, previousSource;
    int wheelRemainderX = 0, wheelRemainderY = 0;
    likely_env root = likely_standard(likely_default_settings(likely_file_void, false), NULL, likely_file_void);
    likely_env current = NULL;

public:
    Source()
    {
        setLineWrapMode(QPlainTextEdit::NoWrap);
        connect(this, SIGNAL(textChanged()), this, SLOT(eval()));
        connect(CommandMode::get(), SIGNAL(changed(bool)), this, SLOT(commandMode(bool)));
    }

    ~Source()
    {
        likely_release_env(current);
        likely_release_env(root);
    }

public slots:
    void setHeader(const QString &header)
    {
        this->header = header;
        eval();
    }

    void setExecution(const QString &execution)
    {
        likely_settings settings = likely_default_settings(likely_file_void, false);
        settings.multicore = (execution == "Multi-core");
        likely_release_env(root);
        root = likely_standard(settings, NULL, likely_file_void);
        previousSource.clear(); // clear cache
        eval();
    }

    void commandsMenu(QAction *a)
    {
        if (a->text().startsWith("Increment") ||
            a->text().startsWith("Decrement")) {
            QTextCursor tc = textCursor();
            bool ok;
            int n = selectNumber(tc, &ok);
            if (ok) {
                int scale = 0, add = 0;
                if      (a->text() == "Increment")     add = 1;
                else if (a->text() == "Decrement")     add = -1;
                else if (a->text() == "Increment 10x") scale = 1;
                else if (a->text() == "Decrement 10x") scale = -1;
                tc.insertText(QString::number(qRound(qPow(10,scale)*n+add)));
            }
        }
    }

private:
    void updateHotSpot(const QPoint &pos)
    {
        likely_const_env hotSpot = NULL;
        if (current && !pos.isNull()) {
            const QTextCursor tc = cursorForPosition(pos);
            const uint32_t line   = uint32_t(tc.blockNumber());
            const uint32_t column = uint32_t(tc.positionInBlock());

            likely_const_env it = current;
            while (it && it->ast &&
                   ((it->ast->begin_line > line) ||
                    ((it->ast->begin_line == line) && (it->ast->begin_column > column))))
                it = it->parent;

            if (it && it->ast &&
                ((it->ast->end_line > line) ||
                 ((it->ast->end_line == line) && (it->ast->end_column >= column))))
                hotSpot = it;
        }
        emit newHotSpot(hotSpot);
    }

    void mouseMoveEvent(QMouseEvent *e)
    {
        if (e->modifiers() != Qt::ControlModifier)
            return QPlainTextEdit::mouseMoveEvent(e);
        e->accept();
        updateHotSpot(e->pos());
    }

    int selectNumber(QTextCursor &tc, bool *ok)
    {
        int n = selectValUnderCursor(tc, ok);
        if (!*ok) {
            tc.movePosition(QTextCursor::NextWord);
            n = selectValUnderCursor(tc, ok);
        }
        if (!*ok) {
            tc.movePosition(QTextCursor::PreviousWord, QTextCursor::MoveAnchor, 2);
            n = selectValUnderCursor(tc, ok);
        }
        return n;
    }

    void wheelEvent(QWheelEvent *e)
    {
        if (e->modifiers() != Qt::ControlModifier)
            return QPlainTextEdit::wheelEvent(e);

        e->accept();
        const int deltaY =                    getIncrement(e->angleDelta().y(), wheelRemainderY, wheelRemainderX);
        const int deltaX = (deltaY != 0 ? 0 : getIncrement(e->angleDelta().x(), wheelRemainderX, wheelRemainderY));
        if ((deltaX == 0) && (deltaY == 0))
            return;

        QTextCursor tc = cursorForPosition(e->pos());
        bool ok;
        int n = selectNumber(tc, &ok);
        if (ok) tc.insertText(QString::number(qRound(qPow(10, deltaX) * (n + deltaY))));
    }

    static int getIncrement(int delta, int &remainder, int &remainderOther)
    {
        remainder += delta;
        const int increment = remainder / 120;
        if (increment != 0) {
            remainder = remainder % 120;
            remainderOther = 0;
        }
        return increment;
    }

    static int selectValUnderCursor(QTextCursor &tc, bool *ok)
    {
        QTextCursor tcCopy(tc);
        tcCopy.select(QTextCursor::WordUnderCursor);

        // Does the number start with a negative sign?
        QTextCursor tcNegative(tcCopy);
        tcNegative.movePosition(QTextCursor::StartOfWord);
        tcNegative.movePosition(QTextCursor::PreviousCharacter);
        tcNegative.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        tcNegative.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
        if (tcNegative.selectedText().startsWith('-'))
            tcCopy = tcNegative;

        const int val = tcCopy.selectedText().toInt(ok);
        if (*ok) tc = tcCopy;
        return val;
    }

    static void evalCallback(likely_const_env env, void *context)
    {
        reinterpret_cast<Source*>(context)->newResult(env);
    }

    static bool contains(likely_const_ast ast, likely_const_ast sub_ast)
    {
        if (!likely_ast_compare(ast, sub_ast))
            return true;
        if (ast->type == likely_ast_list)
            for (uint32_t i=0; i<ast->num_atoms; i++)
                if (contains(ast->atoms[i], sub_ast))
                    return true;
        return false;
    }

private slots:
    void commandMode(bool enabled)
    {
        setMouseTracking(enabled);
        updateHotSpot(enabled ? mapFromGlobal(QCursor::pos()) : QPoint());
    }

    void eval()
    {
        // This check needed because syntax highlighting triggers a textChanged() signal
        const QString source = header + toPlainText();
        if (source == previousSource) return;
        else                          previousSource = source;

        QElapsedTimer elapsedTimer;
        elapsedTimer.start();

        const likely_ast source_ast = likely_lex_and_parse(qPrintable(toPlainText()), likely_file_gfm);
        if (!source_ast)
            return;

        const likely_ast header_ast = likely_lex_and_parse(qPrintable(header), likely_file_gfm);
        for (uint32_t i=0; i<header_ast->num_atoms; i++) {
            // Remove unused variables
            if (!contains(source_ast, header_ast->atoms[i]->atoms[1])) {
                likely_release_ast(header_ast->atoms[i]);
                const_cast<likely_ast*>(header_ast->atoms)[i] = NULL;
            }
        }

        likely_env env = likely_eval(header_ast, root, NULL, NULL);
        likely_release_ast(header_ast);

        env = likely_eval(source_ast, env, evalCallback, this);
        likely_release_ast(source_ast);
        if (env->expr) {
            const qint64 nsec = elapsedTimer.nsecsElapsed();
            emit newStatus(QString("Evaluation Speed: %1 Hz").arg(nsec == 0 ? QString("infinity") : QString::number(double(1E9)/nsec, 'g', 3)));
        }
        likely_release_env(current);
        current = env;
        emit finishedEval(toPlainText());
    }

signals:
    void finishedEval(QString);
    void newHotSpot(likely_const_env);
    void newResult(likely_const_env);
    void newStatus(QString);
};

struct Image : public QGLWidget
{
    QImage image;

    Image(QWidget *parent)
        : QGLWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setVisible(false);
    }

    void setImage(const QImage &image)
    {
        this->image = image;
        setVisible(!image.isNull());
        updateGeometry();
        update();
    }

private:
    QSize sizeHint() const
    {
        return QSize(width(), (image.height() * width() + /* round up */ image.width() - 1) / image.width());
    }

    void paintEvent(QPaintEvent *e)
    {
        e->accept();
        QPainter painter(this);
        painter.drawImage(rect(), image);
    }

    void resizeEvent(QResizeEvent *event)
    {
        QGLWidget::resizeEvent(event);
        event->accept();
        updateGeometry();
    }
};

class Matrix : public QFrame
{
    Q_OBJECT
    QString name;
    QPoint prevMousePos;
    int width = 0, height = 0;
    double x = 0, y = 0, angle = 0, scale = 1;
    QLabel *type, *definition;
    Image *image;
    QVBoxLayout *layout;

public:
    Matrix()
        : type(new QLabel(this))
        , definition(new QLabel(this))
        , image(new Image(this))
        , layout(new QVBoxLayout(this))
    {
        definition->setWordWrap(true);
        layout->addWidget(type);
        layout->addWidget(image);
        layout->addWidget(definition);
        layout->setSpacing(3);
        grabGesture(Qt::PinchGesture);
        setLayout(layout);
        setLineWidth(2);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setVisible(false);
        spartan(false);
    }

    void show(likely_const_mat m, const QString &name)
    {
        if (!m)
            return hide();

        if ((m->channels * m->columns * m->rows * m->frames <= 16) || likely_is_string(m)) {
            image->setImage(QImage());

            likely_mat printed = likely_to_string(m);
            type->setText(printed->data);
            likely_release_mat(printed);
        } else {
            likely_mat show = (m->frames == 1) ? likely_retain_mat(m)
                                               : likely_new(m->type & ~likely_multi_frame, m->channels, m->columns, m->rows, 1,
                                                            m->data + size_t(min(fabs(x), 1.0) * (m->frames-1)) * (((m->type & likely_depth) * size_t(m->channels) * size_t(m->columns) * size_t(m->rows) + 7) / 8));
            double min, max;
            likely_const_mat rendered = likely_render(show, &min, &max);
            likely_release_mat(show);
            image->setImage(QImage(reinterpret_cast<const uchar*>(rendered->data), rendered->columns, rendered->rows, 3*rendered->columns, QImage::Format_RGB888).rgbSwapped());
            likely_release_mat(rendered);

            likely_mat str = likely_type_to_string(m->type);
            type->setText(QString("%1 (%2 %3 %4 %5) [%6, %7]").arg(str->data,
                                                                   QString::number(m->channels),
                                                                   QString::number(m->columns),
                                                                   QString::number(m->rows),
                                                                   QString::number(m->frames),
                                                                   QString::number(min),
                                                                   QString::number(max)));
            likely_release_mat(str);
        }

        updateDefinition(name);
        setVisible(true);
    }

    QString getDefinition() const
    {
        return definition->text();
    }

    void spartan(bool enabled)
    {
        type->setVisible(!enabled);
        if (enabled) {
            layout->setContentsMargins(0, 0, 0, 0);
            setFrameStyle(QFrame::NoFrame);
        } else {
            layout->setContentsMargins(3, 3, 3, 3);
            setFrameStyle(QFrame::Panel | QFrame::Raised);
        }
        updateDefinitionVisibility();
    }

    void reset()
    {
        if ((x == 0) && (y == 0) && (angle == 0) && (scale == 1))
            return;
        x = y = angle = 0;
        scale = 1;
        updateDefinition(name, true);
    }

private:
    bool event(QEvent *e)
    {
        if (e->type() == QEvent::Gesture) {
            QGestureEvent *ge = static_cast<QGestureEvent*>(e);
            if (QPinchGesture *pinch = static_cast<QPinchGesture*>(ge->gesture(Qt::PinchGesture))) {
                scale = scale / pinch->scaleFactor() * pinch->lastScaleFactor();
                angle += pinch->rotationAngle() - pinch->lastRotationAngle();
                updateDefinition(name, true);
            }
            return true;
        }

        return QWidget::event(e);
    }

    void mouseDoubleClickEvent(QMouseEvent *e)
    {
        e->accept();
        QPoint point = image->mapFromParent(e->pos());
        if (!image->rect().contains(point))
            return;

        const double resolution = qMax(image->size().width(), image->size().height());
        x += (point.x() - image->size().width()  / 2.0) / resolution * scale;
        y += (point.y() - image->size().height() / 2.0) / resolution * scale;
        scale /= 1.5;
        updateDefinition(name, true);
    }

    void mousePressEvent(QMouseEvent *e)
    {
        e->accept();
        if (image->rect().contains(image->mapFromParent(e->pos()))) prevMousePos = e->pos();
        else                                                        prevMousePos = QPoint();
    }

    void mouseMoveEvent(QMouseEvent *e)
    {
        e->accept();
        QPoint mousePos = e->pos();
        if (!prevMousePos.isNull()) {
            const double resolution = qMax(image->size().width(), image->size().height());
            x -= (mousePos.x() - prevMousePos.x()) / resolution * scale;
            y -= (mousePos.y() - prevMousePos.y()) / resolution * scale;
        }
        prevMousePos = mousePos;
        updateDefinition(name, true);
    }

    void resizeEvent(QResizeEvent *e)
    {
        QWidget::resizeEvent(e);
        e->accept();
        updateDefinition(name);
    }

    void wheelEvent(QWheelEvent *e)
    {
        e->accept();
        const double delta = double(e->delta()) / (360 * 8);
        if (e->orientation() == Qt::Horizontal) x += delta * scale;
        else                                    y += delta * scale;
        updateDefinition(name, true);
    }

    void updateDefinition(const QString &newName, bool forceUpdate = false)
    {
        const int newWidth  = image->image.isNull() ? 0 : image->size().width();
        const int newHeight = image->image.isNull() ? 0 : image->size().height();

        // Determine if the definition has changed
        bool same = ((name == newName) && (width == newWidth) && (height == newHeight));
        same = same || (name.isEmpty() && newName.isEmpty());
        same = same || (((width == 0) || (height == 0)) && ((newWidth == 0) || (newHeight == 0)));
        same = same && !forceUpdate;

        // Update the definition
        name = newName;
        width = newWidth;
        height = newHeight;

        if (same)
            return;

        if (name.isEmpty() || (width == 0) || (height == 0)) {
            definition->clear();
        } else {
            definition->setText(QString("    %1_x     := %2\n"
                                        "    %1_y     := %3\n"
                                        "    %1_width := %4\n"
                                        "    %1_height:= %5\n"
                                        "    %1_angle := %6\n"
                                        "    %1_scale := %7").arg(name,
                                                                  QString::number(x),
                                                                  QString::number(y),
                                                                  QString::number(width),
                                                                  QString::number(height),
                                                                  QString::number(angle),
                                                                  QString::number(scale)));
        }

        updateDefinitionVisibility();
        emit definitionChanged();
    }

    void updateDefinitionVisibility()
    {
        definition->setVisible(!definition->text().isEmpty() &&
                               (frameStyle() != QFrame::NoFrame)); // don't show in spartan mode
    }

signals:
    void definitionChanged();
};

class Printer : public QScrollArea
{
    Q_OBJECT
    QVBoxLayout *layout;
    Matrix *hotSpot;
    int offset = 1;
    bool dirty = false;

public:
    Printer(QWidget *parent = 0)
        : QScrollArea(parent)
    {
        setFrameShape(QFrame::NoFrame);
        setWidget(new QWidget());
        setWidgetResizable(true);
        layout = new QVBoxLayout(this);
        layout->setAlignment(Qt::AlignTop);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);
        hotSpot = new Matrix();
        layout->addWidget(hotSpot);
        widget()->setLayout(layout);
    }

    void spartan(bool enabled)
    {
        assert(offset == 1);
        while (QLayoutItem *item = layout->itemAt(offset++))
            static_cast<Matrix*>(item->widget())->spartan(enabled);
        finishedPrinting();
    }

public slots:
    void setHotSpot(likely_const_env env)
    {
        if (env)
            hotSpot->show(likely_result(env->expr), likely_symbol(env->ast));
        else
            hotSpot->hide();
    }

    void print(likely_const_env env)
    {
        if (likely_is_definition(env->ast))
            return;

        if (const likely_const_mat m = likely_result(env->expr)) {
            const QString name = likely_symbol(env->ast);
            const int i = offset++;
            if (QLayoutItem *item = layout->itemAt(i)) // Try to recycle the widget
                return static_cast<Matrix*>(item->widget())->show(m, name);
            Matrix *matrix = new Matrix();
            layout->insertWidget(i, matrix);
            connect(matrix, SIGNAL(definitionChanged()), this, SLOT(definitionChanged()));
            matrix->show(m, name);
        }
    }

    void finishedPrinting()
    {
        for (int i=offset; i<layout->count(); i++)
            layout->itemAt(i)->widget()->deleteLater();
        offset = 1;
        if (dirty) {
            definitionChanged();
            dirty = false;
        }
    }

    void reset()
    {
        assert(offset == 1);
        while (QLayoutItem *item = layout->itemAt(offset++))
            static_cast<Matrix*>(item->widget())->reset();
        finishedPrinting();
    }

private slots:
    void definitionChanged()
    {
        if (offset > 1) {
            // Wait until printing is done to emit new definitions
            dirty = true;
        } else {
            QStringList definitions;
            for (int i=1; i<layout->count(); i++)
                definitions.append(static_cast<Matrix*>(layout->itemAt(i)->widget())->getDefinition());
            definitions.removeAll(QString());
            emit newDefinitions(definitions.join("\n") + (definitions.isEmpty() ? "" : "\n"));
        }
    }

signals:
    void newDefinitions(QString);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
    QSettings settings;
    Source *source;
    Printer *printer;
    QComboBox *execution;

public:
    MainWindow(QApplication &application)
        : source(new Source())
        , printer(new Printer())
        , execution(new QComboBox())
    {
        QMenu *fileMenu = new QMenu("File");
        QAction *newSource = new QAction("New...", fileMenu);
        QAction *openSource = new QAction("Open...", fileMenu);
        QAction *saveSource = new QAction("Save", fileMenu);
        QAction *saveSourceAs = new QAction("Save As...", fileMenu);
        newSource->setShortcut(QKeySequence("Ctrl+N"));
        openSource->setShortcut(QKeySequence("Ctrl+O"));
        saveSource->setShortcut(QKeySequence("Ctrl+S"));
        saveSourceAs->setShortcut(QKeySequence("Ctrl+Shift+S"));
        fileMenu->addAction(newSource);
        fileMenu->addAction(openSource);
        fileMenu->addAction(saveSource);
        fileMenu->addAction(saveSourceAs);

        QMenu *commandsMenu = new QMenu("Commands");
        QAction *fullScreen = new QAction("Full Screen", commandsMenu);
        QAction *spartan = new QAction("Spartan", commandsMenu);
        QAction *reset = new QAction("Reset", commandsMenu);
        QAction *increment = new QAction("Increment", commandsMenu);
        QAction *decrement = new QAction("Decrement", commandsMenu);
        QAction *increment10x = new QAction("Increment 10x", commandsMenu);
        QAction *decrement10x = new QAction("Decrement 10x", commandsMenu);
        fullScreen->setCheckable(true);
        fullScreen->setShortcut(QKeySequence(Qt::CTRL+Qt::Key_Y));
        spartan->setCheckable(true);
        spartan->setShortcut(QKeySequence(Qt::CTRL+Qt::Key_T));
        reset->setShortcut(QKeySequence(Qt::CTRL+Qt::Key_R));
        increment->setShortcut(QKeySequence(Qt::CTRL+Qt::Key_Equal));
        decrement->setShortcut(QKeySequence(Qt::CTRL+Qt::Key_Minus));
        increment10x->setShortcut(QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_Equal));
        decrement10x->setShortcut(QKeySequence(Qt::CTRL+Qt::SHIFT+Qt::Key_Minus));
        commandsMenu->addAction(fullScreen);
        commandsMenu->addAction(spartan);
        commandsMenu->addAction(reset);
        commandsMenu->addSeparator();
        commandsMenu->addAction(increment);
        commandsMenu->addAction(decrement);
        commandsMenu->addAction(increment10x);
        commandsMenu->addAction(decrement10x);

        QMenuBar *menuBar = new QMenuBar();
        menuBar->addMenu(fileMenu);
        menuBar->addMenu(commandsMenu);

        execution->addItems(QStringList() << "Single-core" << "Multi-core");
        QStatusBar *statusBar = new QStatusBar();
        statusBar->addPermanentWidget(execution);
        statusBar->setSizeGripEnabled(true);

        application.installEventFilter(CommandMode::get());

        SyntaxHighlighter *syntaxHighlighter = new SyntaxHighlighter(source->document());

        const int WindowWidth = 600;
        QSplitter *splitter = new QSplitter(Qt::Horizontal);
        splitter->addWidget(source);
        splitter->addWidget(printer);
        splitter->setSizes(QList<int>() << WindowWidth/2 << WindowWidth/2);

        setCentralWidget(splitter);
        setMenuBar(menuBar);
        setStatusBar(statusBar);
        setWindowTitle("Likely");
        resize(800, WindowWidth);

        connect(execution, SIGNAL(activated(QString)), source, SLOT(setExecution(QString)));
        connect(printer, SIGNAL(newDefinitions(QString)), source, SLOT(setHeader(QString)));
        connect(fileMenu, SIGNAL(triggered(QAction*)), this, SLOT(fileMenu(QAction*)));
        connect(commandsMenu, SIGNAL(triggered(QAction*)), source, SLOT(commandsMenu(QAction*)));
        connect(source, SIGNAL(finishedEval(QString)), this, SLOT(finishedEval(QString)));
        connect(source, SIGNAL(finishedEval(QString)), printer, SLOT(finishedPrinting()));
        connect(source, SIGNAL(newHotSpot(likely_const_env)), printer, SLOT(setHotSpot(likely_const_env)));
        connect(source, SIGNAL(newResult(likely_const_env)), syntaxHighlighter, SLOT(highlight(likely_const_env)));
        connect(source, SIGNAL(newResult(likely_const_env)), printer, SLOT(print(likely_const_env)));
        connect(source, SIGNAL(newStatus(QString)), statusBar, SLOT(showMessage(QString)));
        connect(fullScreen, SIGNAL(toggled(bool)), this, SLOT(fullScreen(bool)));
        connect(spartan, SIGNAL(toggled(bool)), this, SLOT(spartan(bool)));
        connect(reset, SIGNAL(triggered()), printer, SLOT(reset()));

        likely_set_error_callback(error_callback, statusBar);
        restore();
        this->spartan(DreamSpartan);
    }

private slots:
    void fullScreen(bool enabled)
    {
        if (enabled) showFullScreen();
        else         showNormal();
    }

    void spartan(bool enabled)
    {
        source->setVisible(!enabled);
        printer->spartan(enabled);
    }

    void stateChanged()
    {
        if ((windowTitle() != "Likely") && !windowTitle().endsWith("*"))
            setWindowTitle(windowTitle() + "*");
    }

    void restore()
    {
        if (!DreamInput.empty()) {
            settings.setValue("sourceFileName", QString::fromStdString(DreamInput));
            settings.sync();
        }

        // Try to open the previous file
        if (fileMenu("Open Quiet"))
            return;

        const QString code = settings.value("source").toString();
        settings.setValue("source", QString()); // Start empty the next time if this source code crashes
        settings.sync();
        source->setPlainText(code);
    }

    bool fileMenu(const QString &action)
    {
        QString sourceFileName = settings.value("sourceFileName").toString();
        QString sourceFilePath = settings.value("sourceFilePath").toString();

        // Start empty the next time if this source code crashes
        settings.setValue("sourceFileName", QString());
        settings.sync();

        if (action.startsWith("New")) {
            sourceFileName.clear();
            source->setPlainText("");
        } else if (action.startsWith("Open")) {
            if (!action.endsWith("Quiet"))
                sourceFileName = QFileDialog::getOpenFileName(this, "Open Source File", sourceFilePath);
            if (!sourceFileName.isEmpty()) {
                QFile file(sourceFileName);
                if (file.open(QFile::ReadOnly | QFile::Text))
                    source->setPlainText(QString::fromLocal8Bit(file.readAll()));
                else
                    sourceFileName.clear();
            }
        } else {
            if (sourceFileName.isEmpty() || action.startsWith("Save As"))
                sourceFileName = QFileDialog::getSaveFileName(this, "Save Source File", sourceFilePath);
            if (!sourceFileName.isEmpty()) {
                QFile file(sourceFileName);
                file.open(QFile::WriteOnly | QFile::Text);
                file.write(source->toPlainText().toLocal8Bit());
            }
        }

        if (!sourceFileName.isEmpty())
            sourceFilePath = QFileInfo(sourceFileName).filePath();

        settings.setValue("sourceFileName", sourceFileName);
        settings.setValue("sourceFilePath", sourceFilePath);
        settings.sync();
        setWindowTitle(sourceFileName.isEmpty() ? "Likely" : QFileInfo(sourceFileName).fileName());
        return !sourceFileName.isEmpty();
    }

    void fileMenu(QAction *a)
    {
        fileMenu(a->text());
    }

    void finishedEval(const QString &source)
    {
        settings.setValue("source", source);
    }

private:
    static void error_callback(likely_err err, void *context)
    {
        likely_mat str = likely_err_to_string(err);
        qDebug() << str->data;
        reinterpret_cast<QStatusBar*>(context)->showMessage(str->data);
        likely_release_mat(str);
    }
};

int main(int argc, char *argv[])
{
    cl::ParseCommandLineOptions(argc, argv);

    QApplication::setApplicationName("Dream");
    QApplication::setOrganizationName("Likely");
    QApplication::setOrganizationDomain("liblikely.org");
    QApplication application(argc, argv);
    application.setStyleSheet("QWidget { font-family: Monaco }\n");

    MainWindow mainWindow(application);
    mainWindow.show();

    const int result = application.exec();
    likely_shutdown();
    return result;
}

#ifdef LIKELY_STATIC_QT
#  ifdef Q_OS_OSX
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
#  endif // Q_OS_OSX
#  ifdef Q_OS_LINUX
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#  endif // Q_OS_LINUX
#endif // LIKELY_STATIC_QT

#include "dream.moc"

/// \endcond
