#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>
#include <algorithm>

using json = nlohmann::json;

struct PartType
{
    int PartTypeId;
    double Height;
    double Length;
    double Width;
    double Area;
    double Volume;
};

struct OrderDetail
{
    PartType *PartType;
    int Quantity;
    int Material;
    int Quality;
    int OrderId; // 添加 OrderId 成员变量
};

struct Order
{
    int OrderId;
    double DueDate;
    double ReleaseDate;
    double PenaltyCost;
    std::vector<OrderDetail> OrderList;
};

struct Machine
{
    int MachineId;
    double Area, Height, Length, Width;
    std::vector<int> Materials;
    std::vector<std::vector<double>> MaterialSetup;
    std::vector<double> StartSetup;
    double ScanTime, RecoatTime, RemovalTime;
};

struct Batch
{
    int materialType;
    std::vector<PartType> parts;
    double totalArea;
};

struct MachineBatch
{
    int MachineId;
    double MachineArea;
    std::vector<Batch> Batches;
};

void printMachine(const Machine &machine)
{
    std::cout << "Machine ID: " << machine.MachineId << ", Area: " << machine.Area
              << ", Height: " << machine.Height << ", Length: " << machine.Length
              << ", Width: " << machine.Width << std::endl;
    std::cout << "\tMaterials: ";
    for (const auto &material : machine.Materials)
    {
        std::cout << material << " ";
    }
    std::cout << "\n\tMaterial Setup Times: ";
    for (const auto &setupTimes : machine.MaterialSetup)
    {
        for (auto time : setupTimes)
        {
            std::cout << time << " ";
        }
        std::cout << "; ";
    }
    std::cout << "\n\tStart Setup Times: ";
    for (const auto &time : machine.StartSetup)
    {
        std::cout << time << " ";
    }
    std::cout << "\n\tScan Time: " << machine.ScanTime
              << "\n\tRecoat Time: " << machine.RecoatTime
              << "\n\tRemoval Time: " << machine.RemovalTime << std::endl;
}

void printPartType(const PartType &part)
{
    std::cout << "PartType ID: " << part.PartTypeId << ", Height: " << part.Height
              << ", Length: " << part.Length << ", Width: " << part.Width
              << ", Area: " << part.Area << ", Volume: " << part.Volume << std::endl;
}
void printOrderDetail(const OrderDetail &detail, const std::map<int, PartType> &partTypes, const std::map<int, Order> &orders)
{
    const PartType &part = partTypes.at(detail.PartType->PartTypeId);
    std::cout << "\tPartType ID: " << part.PartTypeId
              << ", Quantity: " << detail.Quantity
              << ", Material: " << detail.Material
              << ", Quality: " << detail.Quality << std::endl;
    std::cout << "\t\tPartType Details: Height: " << part.Height
              << ", Length: " << part.Length
              << ", Width: " << part.Width
              << ", Area: " << part.Area
              << ", Volume: " << part.Volume << std::endl;

    // 遍歷 orders 映射，尋找包含當前 PartType 的訂單
    for (const auto &orderPair : orders)
    {
        const Order &order = orderPair.second;
        for (const auto &od : order.OrderList)
        {
            if (od.PartType->PartTypeId == detail.PartType->PartTypeId)
            {
                // 當找到包含當前 PartType 的訂單時，打印訂單信息
                std::cout << "\t\t\tFrom Order ID: " << order.OrderId
                          << ", DueDate: " << order.DueDate
                          << ", ReleaseDate: " << order.ReleaseDate
                          << ", PenaltyCost: " << order.PenaltyCost << std::endl;
                // 可能只想打印一次訂單信息，如果是這樣，打印後可以跳出循環
                break;
            }
        }
    }
}

// 在呼叫 printOrderDetail 時傳遞 orders 映射
void printOrder(const Order &order, const std::map<int, PartType> &partTypes, const std::map<int, Order> &orders)
{
    std::cout << "Order ID: " << order.OrderId
              << ", DueDate: " << order.DueDate
              << ", ReleaseDate: " << order.ReleaseDate
              << ", PenaltyCost: " << order.PenaltyCost << std::endl;
    for (const auto &detail : order.OrderList)
    {
        printOrderDetail(detail, partTypes, orders); // 傳遞 partTypes 和 orders 參數
    }
}

bool compareMachines(const std::pair<int, Machine> &a, const std::pair<int, Machine> &b)
{
    double totalTimeA = a.second.ScanTime + a.second.RecoatTime;
    double totalTimeB = b.second.ScanTime + b.second.RecoatTime;
    return totalTimeA > totalTimeB; // Change to < for ascending order
}

std::vector<std::pair<int, Machine>> sortMachines(const std::map<int, Machine> &machines)
{
    // 复制 machines map 到一个向量进行排序
    std::vector<std::pair<int, Machine>> machinesVec(machines.begin(), machines.end());

    // 使用自定义比较器排序向量
    std::sort(machinesVec.begin(), machinesVec.end(), compareMachines);

    return machinesVec;
}

bool orderDetailComparator(const OrderDetail &a, const OrderDetail &b, const std::map<int, Order> &orders, const std::map<int, PartType> &partTypes)
{
    const Order &orderA = orders.at(a.OrderId);
    const Order &orderB = orders.at(b.OrderId);
    const PartType &partA = *a.PartType;
    const PartType &partB = *b.PartType;

    // 首先根据 DueDate 排序
    if (orderA.DueDate != orderB.DueDate)
    {
        return orderA.DueDate < orderB.DueDate;
    }
    // 如果 DueDate 相同，则根据 PenaltyCost 排序
    if (orderA.PenaltyCost != orderB.PenaltyCost)
    {
        return orderA.PenaltyCost > orderB.PenaltyCost;
    }
    // 如果 PenaltyCost 也相同，则根据 Volume 排序
    return partA.Volume > partB.Volume;
}
// Function to sort and print material classified order details
std::map<int, std::vector<OrderDetail>> sortMaterialClassifiedOrderDetails(const std::map<int, std::vector<OrderDetail>> &materialClassifiedOrderDetails, const std::map<int, Order> &orders, const std::map<int, PartType> &partTypes)
{
    std::map<int, std::vector<OrderDetail>> sortedMaterialOrderDetails;

    for (const auto &entry : materialClassifiedOrderDetails)
    {
        auto sortedOrderDetails = entry.second;

        std::sort(sortedOrderDetails.begin(), sortedOrderDetails.end(),
                  [&](const OrderDetail &a, const OrderDetail &b)
                  {
                      return orderDetailComparator(a, b, orders, partTypes);
                  });

        sortedMaterialOrderDetails[entry.first] = sortedOrderDetails;
    }

    return sortedMaterialOrderDetails;
}

std::vector<MachineBatch> createMachineBatches(const std::map<int, std::vector<OrderDetail>> &sortedMaterials, const std::vector<std::pair<int, Machine>> &sortedMachines) {
    std::vector<MachineBatch> machineBatches;

    for (const auto &machinePair : sortedMachines) {
        MachineBatch machineBatch;
        machineBatch.MachineId = machinePair.first;
        machineBatch.MachineArea = machinePair.second.Area;

        double availableArea = machinePair.second.Area; // 当前机器的可用面积
        std::vector<Batch> batches;

        for (const auto &materialEntry : sortedMaterials) {
            int materialType = materialEntry.first;
            double totalArea = 0.0;
            std::vector<PartType> partsInBatch;

            for (const auto &orderDetail : materialEntry.second) {
                double partArea = orderDetail.PartType->Area * orderDetail.Quantity;
                if (totalArea + partArea <= availableArea) {
                    partsInBatch.push_back(*orderDetail.PartType);
                    totalArea += partArea;
                } else {
                    if (!partsInBatch.empty()) {
                        batches.push_back({materialType, partsInBatch, totalArea});
                        availableArea -= totalArea;
                        partsInBatch.clear();
                        totalArea = 0.0;
                    }
                    // 如果单个零件的面积就超过了机器的可用面积，这里需要一些额外的逻辑来处理
                    // ...
                }
            }

            if (!partsInBatch.empty()) {
                batches.push_back({materialType, partsInBatch, totalArea});
            }
        }

        machineBatch.Batches = batches;
        machineBatches.push_back(machineBatch);
    }

    return machineBatches;
}


void read_json(const std::string &file_path)
{
    std::ifstream file(file_path);
    json j;
    file >> j;

    // Read basic parameters
    int instanceID = j["InstanceID"];
    int seedValue = j["SeedValue"];
    int nOrders = j["nOrders"];
    int nItemsPerOrder = j["nItemsPerOrder"];
    int nTotalItems = j["nTotalItems"];
    int nMachines = j["nMachines"];
    int nMaterials = j["nMaterials"];
    double PercentageOfTardyOrders = j["PercentageOfTardyOrders"];
    double DueDateRange = j["DueDateRange"];

    // 解析 Machines 部分
    std::map<int, Machine> machines;
    for (auto &kv : j["Machines"].items())
    {
        int key = std::stoi(kv.key());
        json value = kv.value();
        Machine m;
        m.MachineId = value["MachineId"];
        m.Area = value["Area"];
        m.Height = value["Height"];
        m.Length = value["Length"];
        m.Width = value["Width"];
        for (auto &material : value["Materials"])
        {
            m.Materials.push_back(material);
        }
        for (auto &setup : value["MaterialSetup"])
        {
            m.MaterialSetup.push_back(setup.get<std::vector<double>>());
        }
        // Parsing the StartSetup as a vector of doubles
        m.StartSetup = value["StartSetup"].get<std::vector<double>>();
        // Parsing the ScanTime, RecoatTime, and RemovalTime
        m.ScanTime = value["ScanTime"];
        m.RecoatTime = value["RecoatTime"];
        m.RemovalTime = value["RemovalTime"];
        machines[key] = m;
        // printMachine(m); // Print Machine information
    }

    auto sortedMachines = sortMachines(machines); // 排序好的機器

    // 解析 PartTypes 部分
    std::map<int, PartType> partTypes;
    for (auto &kv : j["PartTypes"].items())
    {
        int key = std::stoi(kv.key());
        json value = kv.value();
        PartType p;
        p.PartTypeId = key;
        p.Height = value["Height"];
        p.Length = value["Length"];
        p.Width = value["Width"];
        p.Area = value["Area"];
        p.Volume = value["Volume"];
        partTypes[key] = p;
        // printPartType(p);  // 打印 PartType 信息
    }
    // 解析 Orders 部分
    std::map<int, Order> orders;
    std::map<int, std::vector<OrderDetail>> materialClassifiedOrderDetails;
    for (auto &kv : j["Orders"].items())
    {
        int key = std::stoi(kv.key());
        json value = kv.value();
        Order o;
        o.OrderId = value["OrderId"];
        o.DueDate = value["DueDate"];
        o.ReleaseDate = value["ReleaseDate"];
        o.PenaltyCost = value["PenaltyCost"];
        for (auto &detailValue : value["OrderList"])
        {
            OrderDetail od;
            int partTypeId = detailValue["PartType"];
            od.PartType = &partTypes[partTypeId]; // Reference to PartType structure
            od.Quantity = detailValue["Quantity"];
            od.Material = detailValue["Material"];
            od.Quality = detailValue["Quality"];
            od.OrderId = o.OrderId; // 设置 OrderId
            materialClassifiedOrderDetails[od.Material].push_back(od);
            o.OrderList.push_back(od);
            // printOrderDetail(od, partTypes, orders);
        }
        orders[o.OrderId] = o;
        // printOrder(o, partTypes, orders);
    }

    // 打印按 Material 分類的訂單細節
    // Printing material classified order details
    for (const auto &entry : materialClassifiedOrderDetails)
    {
        // std::cout << "Material: " << entry.first << std::endl;
        for (const auto &detail : entry.second)
        {
            // Correctly passing all required arguments to the printOrderDetail function
            // printOrderDetail(detail, partTypes, orders); // Pass the maps partTypes and orders here
        }
    }

    auto sortedMaterials = sortMaterialClassifiedOrderDetails(materialClassifiedOrderDetails, orders, partTypes);

    auto machineBatches = createMachineBatches(sortedMaterials, sortedMachines);

    // 打印机器和分配的批次信息
    for (const auto &machineBatch : machineBatches)
    {
        std::cout << "Machine ID: " << machineBatch.MachineId << ", Area: " << machineBatch.MachineArea << std::endl;
        for (const auto &batch : machineBatch.Batches)
        {
            std::cout << "\tMaterial Type: " << batch.materialType << ", Total Area: " << batch.totalArea << std::endl;
            for (const auto &part : batch.parts)
            {
                std::cout << "\t\tPart Type: " << part.PartTypeId << ", Area: " << part.Area << std::endl;
            }
        }
    }
}

int main()
{
    std::vector<std::string> files = {"file.json"}; // Replace with your file name
    for (const auto &file : files)
    {
        read_json(file);
    }
    return 0;
}
