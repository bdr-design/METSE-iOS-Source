import UIKit

struct GatewayModule {
    enum Destination {
        case game
        case training
        case operations
        case loadout
        case updates
        case settings
        case diagnostics
    }

    let title: String
    let subtitle: String
    let symbol: String
    let destination: Destination

    static let secondary: [GatewayModule] = [
        .init(title: "التدريب", subtitle: "حركة، رماية، تكتيك", symbol: "scope", destination: .training),
        .init(title: "العمليات", subtitle: "المهام والجلسات", symbol: "map", destination: .operations),
        .init(title: "العتاد", subtitle: "الأسلحة والتجهيز", symbol: "shield.lefthalf.filled", destination: .loadout),
        .init(title: "مركز التحديث", subtitle: "مؤجل حتى اكتمال البناء الأساسي", symbol: "arrow.triangle.2.circlepath", destination: .updates),
        .init(title: "الإعدادات", subtitle: "العرض، الصوت، التحكم", symbol: "slider.horizontal.3", destination: .settings),
        .init(title: "مركز الأرصاد", subtitle: "رصد الأداء، المحرك، التصادم والسلامة", symbol: "waveform.path.ecg", destination: .diagnostics)
    ]
}
