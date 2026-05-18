#include "scheduler.h"
#include "service_graph.h"

#include <QApplication>
#include <QColor>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QLineF>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
std::string readFileText(const std::string& path) {
    std::ifstream file(path);
    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

bool writeFileText(const std::string& path, const QString& text, std::string& error) {
    std::ofstream file(path);
    if (!file) {
        error = "Unable to write file: " + path;
        return false;
    }
    file << text.toStdString();
    return true;
}

std::string trim(const std::string& input) {
    const std::string whitespace = " \t\r\n";
    const size_t start = input.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }
    const size_t end = input.find_last_not_of(whitespace);
    return input.substr(start, end - start + 1);
}

std::vector<std::string> split(const std::string& input, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream stream(input);
    std::string item;
    while (std::getline(stream, item, delimiter)) {
        parts.push_back(trim(item));
    }
    return parts;
}

void setTableHeaders(QTableWidget* table, const QStringList& headers) {
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
}

void populateServiceTable(QTableWidget* table, const std::string& fileText) {
    table->setRowCount(0);
    std::stringstream stream(fileText);
    std::string line;
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::vector<std::string> fields = split(line, '|');
        if (fields.size() != 3) {
            continue;
        }
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(fields[0])));
        table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(fields[1])));
        table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(fields[2])));
    }
}

void populateJobsTable(QTableWidget* table, const std::string& fileText) {
    table->setRowCount(0);
    std::stringstream stream(fileText);
    std::string line;
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::vector<std::string> fields = split(line, '|');
        if (fields.size() != 4) {
            continue;
        }
        const int row = table->rowCount();
        table->insertRow(row);
        for (int col = 0; col < 4; ++col) {
            table->setItem(row, col, new QTableWidgetItem(QString::fromStdString(fields[col])));
        }
    }
}

QString serializeServiceTable(QTableWidget* table) {
    QStringList lines;
    lines << "# format: service_name|critical(yes/no)|dependency1,dependency2 or -";
    for (int row = 0; row < table->rowCount(); ++row) {
        const QString name = table->item(row, 0) ? table->item(row, 0)->text().trimmed() : "";
        const QString critical = table->item(row, 1) ? table->item(row, 1)->text().trimmed() : "";
        QString deps = table->item(row, 2) ? table->item(row, 2)->text().trimmed() : "";
        if (name.isEmpty()) {
            continue;
        }
        if (deps.isEmpty()) {
            deps = "-";
        }
        lines << (name + "|" + critical + "|" + deps);
    }
    return lines.join("\n") + "\n";
}

QString serializeJobsTable(QTableWidget* table) {
    QStringList lines;
    lines << "# format: job_name|arrival_time|burst_time|priority";
    for (int row = 0; row < table->rowCount(); ++row) {
        QStringList fields;
        bool nonEmpty = false;
        for (int col = 0; col < table->columnCount(); ++col) {
            const QString value = table->item(row, col) ? table->item(row, col)->text().trimmed() : "";
            fields << value;
            nonEmpty = nonEmpty || !value.isEmpty();
        }
        if (!nonEmpty) {
            continue;
        }
        lines << fields.join("|");
    }
    return lines.join("\n") + "\n";
}

QWidget* buildTableTab(
    QTableWidget* table,
    QPushButton* addButton,
    QPushButton* removeButton,
    QPushButton* upButton,
    QPushButton* downButton
) {
    auto* wrapper = new QWidget();
    auto* layout = new QVBoxLayout(wrapper);
    auto* controls = new QHBoxLayout();
    controls->addWidget(addButton);
    controls->addWidget(removeButton);
    controls->addWidget(upButton);
    controls->addWidget(downButton);
    controls->addStretch();
    layout->addLayout(controls);
    layout->addWidget(table);
    return wrapper;
}

void ensureServiceRow(QTableWidget* table, int row, const QString& name = "", const QString& critical = "no", const QString& deps = "-") {
    table->insertRow(row);
    table->setItem(row, 0, new QTableWidgetItem(name));
    table->setItem(row, 1, new QTableWidgetItem(critical));
    table->setItem(row, 2, new QTableWidgetItem(deps));
}

void ensureJobRow(
    QTableWidget* table,
    int row,
    const QString& name = "",
    const QString& arrival = "0",
    const QString& burst = "1",
    const QString& priority = "5"
) {
    table->insertRow(row);
    table->setItem(row, 0, new QTableWidgetItem(name));
    table->setItem(row, 1, new QTableWidgetItem(arrival));
    table->setItem(row, 2, new QTableWidgetItem(burst));
    table->setItem(row, 3, new QTableWidgetItem(priority));
}

void moveSelectedRow(QTableWidget* table, int delta) {
    const int row = table->currentRow();
    if (row < 0) {
        return;
    }
    const int target = row + delta;
    if (target < 0 || target >= table->rowCount()) {
        return;
    }

    QStringList currentValues;
    QStringList targetValues;
    for (int col = 0; col < table->columnCount(); ++col) {
        currentValues << (table->item(row, col) ? table->item(row, col)->text() : "");
        targetValues << (table->item(target, col) ? table->item(target, col)->text() : "");
    }
    for (int col = 0; col < table->columnCount(); ++col) {
        table->item(row, col)->setText(targetValues[col]);
        table->item(target, col)->setText(currentValues[col]);
    }
    table->selectRow(target);
}

std::string bootOrderText(ServiceGraph& graph) {
    std::ostringstream out;
    std::vector<std::string> order;
    std::vector<std::string> cycle;
    if (graph.topologicalSort(order, cycle)) {
        out << "\nValid Boot Order\n";
        out << "----------------\n";
        for (size_t i = 0; i < order.size(); ++i) {
            out << i + 1 << ". " << order[i] << '\n';
        }
    } else {
        out << "\nCycle detected\n";
        out << "--------------\n";
        for (size_t i = 0; i < cycle.size(); ++i) {
            out << cycle[i];
            if (i + 1 < cycle.size()) {
                out << " -> ";
            }
        }
        out << '\n';
    }
    return out.str();
}

std::string fullDemoText(ServiceGraph& normalGraph, ServiceGraph& cycleGraph, PriorityScheduler& scheduler) {
    std::ostringstream out;
    normalGraph.printServices(out);
    out << bootOrderText(normalGraph);
    normalGraph.simulateBoot(out, "AuthenticationService");
    out << "\nCycle Demo\n";
    out << "----------\n";
    cycleGraph.printServices(out);
    out << bootOrderText(cycleGraph);
    scheduler.printJobs(out);
    scheduler.simulate(out, false);
    scheduler.simulate(out, true);
    return out.str();
}

QColor colorForName(const std::string& name) {
    const std::vector<QColor> palette = {
        QColor("#2563eb"), QColor("#059669"), QColor("#d97706"), QColor("#dc2626"),
        QColor("#7c3aed"), QColor("#0891b2"), QColor("#4f46e5"), QColor("#65a30d"),
        QColor("#db2777"), QColor("#0f766e")
    };
    std::size_t hash = std::hash<std::string>{}(name);
    return palette[hash % palette.size()];
}

void fitScene(QGraphicsView* view, QGraphicsScene* scene) {
    scene->setSceneRect(scene->itemsBoundingRect().adjusted(-40, -40, 40, 40));
    view->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
}

void renderServiceGraph(
    QGraphicsView* view,
    QGraphicsScene* scene,
    const ServiceGraph& graph,
    const std::vector<std::string>& highlightedCycle = {},
    const std::unordered_map<std::string, BootState>& bootStates = {}
) {
    scene->clear();

    const auto& services = graph.services();
    if (services.empty()) {
        scene->addText("No services loaded.");
        fitScene(view, scene);
        return;
    }

    std::set<std::string> cycleSet(highlightedCycle.begin(), highlightedCycle.end());
    std::vector<std::string> topoOrder;
    std::vector<std::string> cycleNodes;
    const bool isDag = graph.topologicalSort(topoOrder, cycleNodes);

    std::vector<std::string> names;
    names.reserve(services.size());
    for (const auto& [name, _] : services) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());

    if (!isDag) {
        topoOrder = names;
    }

    std::unordered_map<std::string, int> depth;
    for (const std::string& name : topoOrder) {
        int maxDepth = 0;
        const auto it = services.find(name);
        if (it != services.end()) {
            for (const std::string& dependency : it->second.dependencies) {
                maxDepth = std::max(maxDepth, depth[dependency] + 1);
            }
        }
        depth[name] = maxDepth;
    }
    for (const std::string& name : names) {
        if (!depth.count(name)) {
            depth[name] = 0;
        }
    }

    std::map<int, std::vector<std::string>> layers;
    for (const std::string& name : names) {
        layers[depth[name]].push_back(name);
    }

    std::unordered_map<std::string, QPointF> positions;
    const qreal nodeWidth = 190.0;
    const qreal nodeHeight = 56.0;
    const qreal columnGap = 90.0;
    const qreal rowGap = 34.0;

    int columnIndex = 0;
    for (auto& [layer, layerNames] : layers) {
        Q_UNUSED(layer);
        for (size_t row = 0; row < layerNames.size(); ++row) {
            positions[layerNames[row]] = QPointF(
                columnIndex * (nodeWidth + columnGap),
                row * (nodeHeight + rowGap)
            );
        }
        columnIndex++;
    }

    QPen edgePen(QColor("#94a3b8"));
    edgePen.setWidth(2);
    QPen cycleEdgePen(QColor("#dc2626"));
    cycleEdgePen.setWidth(3);

    for (const auto& [name, node] : services) {
        const QPointF target = positions[name];
        for (const std::string& dependency : node.dependencies) {
            const QPointF source = positions[dependency];
            const bool cycleEdge = cycleSet.count(name) && cycleSet.count(dependency);
            const QLineF line(
                source.x() + nodeWidth,
                source.y() + nodeHeight / 2.0,
                target.x(),
                target.y() + nodeHeight / 2.0
            );
            scene->addLine(line, cycleEdge ? cycleEdgePen : edgePen);

            const double arrowSize = 10.0;
            const double angle = std::atan2(-line.dy(), line.dx());
            const QPointF arrowP1 = line.p2() - QPointF(std::cos(angle + 0.45) * arrowSize, -std::sin(angle + 0.45) * arrowSize);
            const QPointF arrowP2 = line.p2() - QPointF(std::cos(angle - 0.45) * arrowSize, -std::sin(angle - 0.45) * arrowSize);
            QPolygonF arrowHead;
            arrowHead << line.p2() << arrowP1 << arrowP2;
            scene->addPolygon(arrowHead, cycleEdge ? cycleEdgePen : edgePen, QBrush(cycleEdge ? QColor("#dc2626") : QColor("#94a3b8")));
        }
    }

    for (const auto& [name, node] : services) {
        const QPointF pos = positions[name];
        QColor fill = node.critical ? QColor("#fee2e2") : QColor("#e0f2fe");
        QColor border = node.critical ? QColor("#b91c1c") : QColor("#2563eb");
        std::string statusText = node.critical ? "critical" : "normal";
        if (cycleSet.count(name)) {
            fill = QColor("#fecaca");
            border = QColor("#dc2626");
            statusText = "cycle";
        }
        if (bootStates.count(name)) {
            switch (bootStates.at(name)) {
                case BootState::Started:
                    fill = QColor("#dcfce7");
                    border = QColor("#15803d");
                    statusText = "started";
                    break;
                case BootState::Failed:
                    fill = QColor("#fecaca");
                    border = QColor("#b91c1c");
                    statusText = "failed";
                    break;
                case BootState::Skipped:
                    fill = QColor("#fef3c7");
                    border = QColor("#d97706");
                    statusText = "skipped";
                    break;
            }
        }

        scene->addRect(QRectF(pos.x(), pos.y(), nodeWidth, nodeHeight), QPen(border, 2), QBrush(fill));
        auto* label = scene->addSimpleText(QString::fromStdString(name));
        label->setBrush(QBrush(QColor("#0f172a")));
        label->setPos(pos.x() + 10, pos.y() + 8);

        auto* sub = scene->addSimpleText(QString::fromStdString(statusText));
        sub->setBrush(QBrush(QColor("#475569")));
        sub->setPos(pos.x() + 10, pos.y() + 30);
    }

    fitScene(view, scene);
}

void renderTimeline(QGraphicsView* view, QGraphicsScene* scene, const SimulationResult& result) {
    scene->clear();

    if (result.summaries.empty()) {
        scene->addText("No scheduling data available.");
        fitScene(view, scene);
        return;
    }

    const qreal leftMargin = 170.0;
    const qreal topMargin = 48.0;
    const qreal rowHeight = 38.0;
    const qreal laneHeight = 24.0;
    const qreal cellWidth = 32.0;
    const qreal summaryOffset = 24.0;

    std::vector<std::string> jobOrder;
    std::unordered_map<std::string, int> rowIndex;
    for (size_t i = 0; i < result.summaries.size(); ++i) {
        jobOrder.push_back(result.summaries[i].jobName);
        rowIndex[result.summaries[i].jobName] = static_cast<int>(i);
    }

    for (size_t i = 0; i < jobOrder.size(); ++i) {
        const qreal y = topMargin + i * rowHeight;
        auto* label = scene->addSimpleText(QString::fromStdString(jobOrder[i]));
        label->setPos(8, y);
        scene->addLine(leftMargin, y + laneHeight + 2, leftMargin + result.totalTime * cellWidth, y + laneHeight + 2, QPen(QColor("#cbd5e1")));
    }

    for (int t = 0; t <= result.totalTime; ++t) {
        const qreal x = leftMargin + t * cellWidth;
        scene->addLine(x, topMargin - 14, x, topMargin + jobOrder.size() * rowHeight, QPen(QColor("#e2e8f0")));
        auto* tick = scene->addSimpleText(QString::number(t));
        tick->setPos(x - 4, 8);
    }

    for (const SchedulingSlice& slice : result.slices) {
        if (slice.idle || !rowIndex.count(slice.jobName)) {
            continue;
        }
        const int row = rowIndex[slice.jobName];
        const qreal x = leftMargin + slice.startTime * cellWidth;
        const qreal y = topMargin + row * rowHeight;
        const qreal width = std::max(1, slice.duration) * cellWidth;
        QColor color = colorForName(slice.jobName);
        scene->addRect(QRectF(x, y, width, laneHeight), QPen(color.darker(125), 1.5), QBrush(color.lighter(145)));
        auto* text = scene->addSimpleText(QString::fromStdString(slice.jobName));
        text->setBrush(QBrush(QColor("#111827")));
        text->setPos(x + 6, y + 3);
    }

    const qreal summaryX = leftMargin + result.totalTime * cellWidth + summaryOffset;
    scene->addSimpleText("Summary")->setPos(summaryX, 8);
    for (size_t i = 0; i < result.summaries.size(); ++i) {
        const JobSummary& summary = result.summaries[i];
        const qreal y = topMargin + i * rowHeight;
        std::ostringstream line;
        line << "wait " << summary.firstRunWait << " | turn " << summary.turnaround
             << " | " << (summary.starved ? "starved" : "ok");
        auto* text = scene->addSimpleText(QString::fromStdString(line.str()));
        text->setPos(summaryX, y);
    }

    fitScene(view, scene);
}

std::string buildScheduleSummary(const SimulationResult& result) {
    std::ostringstream out;
    out << (result.useAging ? "With aging" : "Without aging") << "\n";
    out << "Policy: ";
    switch (result.policy) {
        case TimeOfDayPolicy::PeakHours:
            out << "Peak Hours";
            break;
        case TimeOfDayPolicy::NightShift:
            out << "Night Shift";
            break;
        case TimeOfDayPolicy::Standard:
        default:
            out << "Standard";
            break;
    }
    out << "\n\n";
    out << "First CPU access is what to point out in the demo.\n";
    out << "Longest wait: " << result.longestWaitJob << " (" << result.longestWait << ")\n\n";
    for (const JobSummary& summary : result.summaries) {
        out << summary.jobName
            << " | base=" << summary.basePriority
            << " | policy=" << summary.adjustedPriority
            << " | first run wait=" << summary.firstRunWait
            << " | total wait=" << summary.totalWait
            << " | turnaround=" << summary.turnaround
            << " | starved=" << (summary.starved ? "yes" : "no") << '\n';
    }
    out << "\nRecent scheduling events\n";
    out << "------------------------\n";
    const size_t start = result.logLines.size() > 14 ? result.logLines.size() - 14 : 0;
    for (size_t i = start; i < result.logLines.size(); ++i) {
        out << result.logLines[i] << '\n';
    }
    return out.str();
}
}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    const std::string servicesPath = "services.txt";
    const std::string cyclePath = "services_cycle.txt";
    const std::string jobsPath = "jobs.txt";

    ServiceGraph normalGraph;
    ServiceGraph cycleGraph;
    PriorityScheduler scheduler;
    std::string error;

    if (!normalGraph.loadFromFile(servicesPath, error) ||
        !cycleGraph.loadFromFile(cyclePath, error) ||
        !scheduler.loadFromFile(jobsPath, error)) {
        QMessageBox::critical(nullptr, "Load Error", QString::fromStdString(error));
        return 1;
    }

    QWidget window;
    window.setWindowTitle("Hospital Infrastructure Scheduler Simulator");
    window.resize(1200, 760);

    auto* rootLayout = new QVBoxLayout(&window);
    auto* topLayout = new QHBoxLayout();
    auto* leftLayout = new QVBoxLayout();
    auto* title = new QLabel("Hospital Infrastructure Scheduler Simulator");
    auto* subtitle = new QLabel("Topological sort, cycle detection, preemptive scheduling, and aging");
    auto* editorTabs = new QTabWidget();
    auto* servicesTable = new QTableWidget();
    auto* cycleTable = new QTableWidget();
    auto* jobsTable = new QTableWidget();
    auto* addServiceRowButton = new QPushButton("Add Row");
    auto* removeServiceRowButton = new QPushButton("Delete Row");
    auto* serviceUpButton = new QPushButton("Move Up");
    auto* serviceDownButton = new QPushButton("Move Down");
    auto* addCycleRowButton = new QPushButton("Add Row");
    auto* removeCycleRowButton = new QPushButton("Delete Row");
    auto* cycleUpButton = new QPushButton("Move Up");
    auto* cycleDownButton = new QPushButton("Move Down");
    auto* addJobRowButton = new QPushButton("Add Row");
    auto* removeJobRowButton = new QPushButton("Delete Row");
    auto* jobUpButton = new QPushButton("Move Up");
    auto* jobDownButton = new QPushButton("Move Down");
    auto* resultTabs = new QTabWidget();
    auto* graphView = new QGraphicsView();
    auto* graphScene = new QGraphicsScene(&window);
    auto* timelineView = new QGraphicsView();
    auto* timelineScene = new QGraphicsScene(&window);
    auto* details = new QPlainTextEdit();

    QFont titleFont;
    titleFont.setPointSize(15);
    titleFont.setBold(true);
    title->setFont(titleFont);

    QFont mono("Monospace");
    mono.setStyleHint(QFont::TypeWriter);
    mono.setPointSize(10);
    details->setReadOnly(true);
    details->setLineWrapMode(QPlainTextEdit::NoWrap);
    details->setFont(mono);
    graphView->setScene(graphScene);
    timelineView->setScene(timelineScene);
    graphView->setRenderHint(QPainter::Antialiasing, true);
    timelineView->setRenderHint(QPainter::Antialiasing, true);
    resultTabs->addTab(graphView, "Graph");
    resultTabs->addTab(timelineView, "Timeline");
    resultTabs->addTab(details, "Details");

    setTableHeaders(servicesTable, {"Service", "Critical", "Dependencies"});
    setTableHeaders(cycleTable, {"Service", "Critical", "Dependencies"});
    setTableHeaders(jobsTable, {"Job", "Arrival", "Burst", "Priority"});
    populateServiceTable(servicesTable, readFileText(servicesPath));
    populateServiceTable(cycleTable, readFileText(cyclePath));
    populateJobsTable(jobsTable, readFileText(jobsPath));

    editorTabs->addTab(
        buildTableTab(servicesTable, addServiceRowButton, removeServiceRowButton, serviceUpButton, serviceDownButton),
        "Normal Services"
    );
    editorTabs->addTab(
        buildTableTab(cycleTable, addCycleRowButton, removeCycleRowButton, cycleUpButton, cycleDownButton),
        "Cycle Demo Services"
    );
    editorTabs->addTab(
        buildTableTab(jobsTable, addJobRowButton, removeJobRowButton, jobUpButton, jobDownButton),
        "Bandwidth Jobs"
    );

    auto setDetails = [&](const std::string& text) {
        details->setPlainText(QString::fromStdString(text));
    };

    auto reloadModels = [&]() -> bool {
        if (!writeFileText(servicesPath, serializeServiceTable(servicesTable), error) ||
            !writeFileText(cyclePath, serializeServiceTable(cycleTable), error) ||
            !writeFileText(jobsPath, serializeJobsTable(jobsTable), error)) {
            QMessageBox::critical(&window, "Save Error", QString::fromStdString(error));
            return false;
        }

        ServiceGraph nextNormalGraph;
        ServiceGraph nextCycleGraph;
        PriorityScheduler nextScheduler;
        if (!nextNormalGraph.loadFromFile(servicesPath, error) ||
            !nextCycleGraph.loadFromFile(cyclePath, error) ||
            !nextScheduler.loadFromFile(jobsPath, error)) {
            QMessageBox::critical(&window, "Input Error", QString::fromStdString(error));
            return false;
        }

        normalGraph = nextNormalGraph;
        cycleGraph = nextCycleGraph;
        scheduler = nextScheduler;
        return true;
    };

    QObject::connect(addServiceRowButton, &QPushButton::clicked, [&]() {
        ensureServiceRow(servicesTable, servicesTable->currentRow() >= 0 ? servicesTable->currentRow() + 1 : servicesTable->rowCount());
    });
    QObject::connect(removeServiceRowButton, &QPushButton::clicked, [&]() {
        if (servicesTable->currentRow() >= 0) {
            servicesTable->removeRow(servicesTable->currentRow());
        }
    });
    QObject::connect(serviceUpButton, &QPushButton::clicked, [&]() { moveSelectedRow(servicesTable, -1); });
    QObject::connect(serviceDownButton, &QPushButton::clicked, [&]() { moveSelectedRow(servicesTable, 1); });

    QObject::connect(addCycleRowButton, &QPushButton::clicked, [&]() {
        ensureServiceRow(cycleTable, cycleTable->currentRow() >= 0 ? cycleTable->currentRow() + 1 : cycleTable->rowCount());
    });
    QObject::connect(removeCycleRowButton, &QPushButton::clicked, [&]() {
        if (cycleTable->currentRow() >= 0) {
            cycleTable->removeRow(cycleTable->currentRow());
        }
    });
    QObject::connect(cycleUpButton, &QPushButton::clicked, [&]() { moveSelectedRow(cycleTable, -1); });
    QObject::connect(cycleDownButton, &QPushButton::clicked, [&]() { moveSelectedRow(cycleTable, 1); });

    QObject::connect(addJobRowButton, &QPushButton::clicked, [&]() {
        ensureJobRow(jobsTable, jobsTable->currentRow() >= 0 ? jobsTable->currentRow() + 1 : jobsTable->rowCount());
    });
    QObject::connect(removeJobRowButton, &QPushButton::clicked, [&]() {
        if (jobsTable->currentRow() >= 0) {
            jobsTable->removeRow(jobsTable->currentRow());
        }
    });
    QObject::connect(jobUpButton, &QPushButton::clicked, [&]() { moveSelectedRow(jobsTable, -1); });
    QObject::connect(jobDownButton, &QPushButton::clicked, [&]() { moveSelectedRow(jobsTable, 1); });

    auto* applyButton = new QPushButton("Apply Current Input");
    auto* resetButton = new QPushButton("Reload From Files");
    auto* graphButton = new QPushButton("Show Service Graph");
    auto* bootButton = new QPushButton("Show Boot Order");
    auto* failButton = new QPushButton("Critical Failure Demo");
    auto* cycleButton = new QPushButton("Cycle Detection Demo");
    auto* jobsButton = new QPushButton("Show Bandwidth Jobs");
    auto* noAgingButton = new QPushButton("Run Without Aging");
    auto* agingButton = new QPushButton("Run With Aging");
    auto* peakHoursButton = new QPushButton("Peak Hours Policy");
    auto* nightShiftButton = new QPushButton("Night Shift Policy");
    auto* fullDemoButton = new QPushButton("Run Full Demo");

    QObject::connect(applyButton, &QPushButton::clicked, [&]() {
        if (reloadModels()) {
            renderServiceGraph(graphView, graphScene, normalGraph);
            resultTabs->setCurrentWidget(graphView);
            setDetails(
                "Input applied successfully.\n\n"
                "You can now run the graph and scheduling demos using your edited data.\n"
                "Typical live flow:\n"
                "1. Edit services or jobs\n"
                "2. Click Apply Current Input\n"
                "3. Run the relevant demo button\n"
            );
        }
    });
    QObject::connect(resetButton, &QPushButton::clicked, [&]() {
        populateServiceTable(servicesTable, readFileText(servicesPath));
        populateServiceTable(cycleTable, readFileText(cyclePath));
        populateJobsTable(jobsTable, readFileText(jobsPath));
        if (reloadModels()) {
            renderServiceGraph(graphView, graphScene, normalGraph);
            resultTabs->setCurrentWidget(graphView);
            setDetails("Editors reloaded from the current files on disk.");
        }
    });
    QObject::connect(graphButton, &QPushButton::clicked, [&]() {
        if (!reloadModels()) {
            return;
        }
        renderServiceGraph(graphView, graphScene, normalGraph);
        resultTabs->setCurrentWidget(graphView);
        std::ostringstream out;
        out << "Dependency graph rendered for the current hospital services.\n";
        out << "Blue nodes are normal services. Red-tinted nodes are critical services.\n";
        out << "Edges point from dependency -> dependent service.\n";
        setDetails(out.str());
    });
    QObject::connect(bootButton, &QPushButton::clicked, [&]() {
        if (!reloadModels()) {
            return;
        }
        renderServiceGraph(graphView, graphScene, normalGraph);
        resultTabs->setCurrentWidget(graphView);
        setDetails(bootOrderText(normalGraph));
    });
    QObject::connect(failButton, &QPushButton::clicked, [&]() {
        if (!reloadModels()) {
            return;
        }
        const BootSimulationResult boot = normalGraph.runBootSimulation("AuthenticationService");
        std::unordered_map<std::string, BootState> states;
        for (const BootStep& step : boot.steps) {
            states[step.serviceName] = step.state;
        }
        renderServiceGraph(graphView, graphScene, normalGraph, {}, states);
        resultTabs->setCurrentWidget(graphView);
        std::ostringstream out;
        normalGraph.simulateBoot(out, "AuthenticationService");
        out << "\nLegend: green=started, red=failed critical service, yellow=skipped dependent services.\n";
        setDetails(out.str());
    });
    QObject::connect(cycleButton, &QPushButton::clicked, [&]() {
        if (!reloadModels()) {
            return;
        }
        std::vector<std::string> order;
        std::vector<std::string> cycle;
        cycleGraph.topologicalSort(order, cycle);
        renderServiceGraph(graphView, graphScene, cycleGraph, cycle);
        resultTabs->setCurrentWidget(graphView);
        std::ostringstream out;
        cycleGraph.printServices(out);
        out << bootOrderText(cycleGraph);
        setDetails(out.str());
    });
    QObject::connect(jobsButton, &QPushButton::clicked, [&]() {
        if (!reloadModels()) {
            return;
        }
        const SimulationResult result = scheduler.runSimulation(false);
        renderTimeline(timelineView, timelineScene, result);
        resultTabs->setCurrentWidget(timelineView);
        std::ostringstream out;
        scheduler.printJobs(out);
        setDetails(out.str());
    });
    QObject::connect(noAgingButton, &QPushButton::clicked, [&]() {
        if (!reloadModels()) {
            return;
        }
        const SimulationResult result = scheduler.runSimulation(false);
        renderTimeline(timelineView, timelineScene, result);
        resultTabs->setCurrentWidget(timelineView);
        setDetails(buildScheduleSummary(result));
    });
    QObject::connect(agingButton, &QPushButton::clicked, [&]() {
        if (!reloadModels()) {
            return;
        }
        const SimulationResult result = scheduler.runSimulation(true);
        renderTimeline(timelineView, timelineScene, result);
        resultTabs->setCurrentWidget(timelineView);
        setDetails(buildScheduleSummary(result));
    });
    QObject::connect(peakHoursButton, &QPushButton::clicked, [&]() {
        if (!reloadModels()) {
            return;
        }
        const SimulationResult result = scheduler.runSimulation(true, TimeOfDayPolicy::PeakHours);
        renderTimeline(timelineView, timelineScene, result);
        resultTabs->setCurrentWidget(timelineView);
        std::string text = buildScheduleSummary(result);
        text += "\nPeak-hours policy: emergency/video/imaging workloads are boosted, while backup/archive/sync workloads are delayed.\n";
        setDetails(text);
    });
    QObject::connect(nightShiftButton, &QPushButton::clicked, [&]() {
        if (!reloadModels()) {
            return;
        }
        const SimulationResult result = scheduler.runSimulation(true, TimeOfDayPolicy::NightShift);
        renderTimeline(timelineView, timelineScene, result);
        resultTabs->setCurrentWidget(timelineView);
        std::string text = buildScheduleSummary(result);
        text += "\nNight-shift policy: backup/archive/sync workloads are boosted for off-hours processing.\n";
        setDetails(text);
    });
    QObject::connect(fullDemoButton, &QPushButton::clicked, [&]() {
        if (!reloadModels()) {
            return;
        }
        renderServiceGraph(graphView, graphScene, normalGraph);
        resultTabs->setCurrentWidget(details);
        setDetails(fullDemoText(normalGraph, cycleGraph, scheduler));
    });

    leftLayout->addWidget(title);
    leftLayout->addWidget(subtitle);
    leftLayout->addSpacing(8);
    leftLayout->addWidget(applyButton);
    leftLayout->addWidget(resetButton);
    leftLayout->addSpacing(8);
    leftLayout->addWidget(graphButton);
    leftLayout->addWidget(bootButton);
    leftLayout->addWidget(failButton);
    leftLayout->addWidget(cycleButton);
    leftLayout->addWidget(jobsButton);
    leftLayout->addWidget(noAgingButton);
    leftLayout->addWidget(agingButton);
    leftLayout->addWidget(peakHoursButton);
    leftLayout->addWidget(nightShiftButton);
    leftLayout->addWidget(fullDemoButton);
    leftLayout->addStretch();

    auto* topPanel = new QWidget();
    topPanel->setLayout(topLayout);
    topLayout->addLayout(leftLayout, 0);
    topLayout->addWidget(editorTabs, 1);

    rootLayout->addWidget(topPanel, 2);
    rootLayout->addWidget(resultTabs, 3);

    renderServiceGraph(graphView, graphScene, normalGraph);
    setDetails(
        "This GUI now supports table-based input and visual output.\n\n"
        "How to use it:\n"
        "1. Edit rows in the tables above\n"
        "2. Click Apply Current Input\n"
        "3. Use Graph or Timeline buttons to render visuals\n\n"
        "Good live demo ideas:\n"
        "- Add a new dependency and regenerate boot order\n"
        "- Create a cycle and show the error immediately\n"
        "- Use Critical Failure Demo to show skipped dependents visually\n"
        "- Compare Peak Hours Policy vs Night Shift Policy on the same jobs\n"
        "- Change burst times and compare starvation behavior\n"
    );

    window.show();
    return app.exec();
}
