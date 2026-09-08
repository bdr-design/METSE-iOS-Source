import UIKit

final class ModulePlaceholderViewController: UIViewController {
    private let destination: GatewayModule.Destination

    init(destination: GatewayModule.Destination) {
        self.destination = destination
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = UIColor(red: 0.01, green: 0.018, blue: 0.020, alpha: 1)
        let title = UILabel(); title.font = .preferredFont(forTextStyle: .largeTitle); title.textColor = .white; title.textAlignment = .center; title.numberOfLines = 0; title.text = displayName
        let note = UILabel(); note.font = .preferredFont(forTextStyle: .body); note.textColor = UIColor.white.withAlphaComponent(0.55); note.textAlignment = .center; note.numberOfLines = 0
        switch destination {
        case .diagnostics:
            note.text = "ابدأ جلسة تكتيكية ثم افتح مركز الأرصاد V2 لرؤية Input Queue، السلاح، المقذوفات، الضرر، Culling، Integrity والحرارة ونسخ التقرير."
        default:
            note.text = "هذه الوحدة محجوزة داخل بنية البوابة وجاهزة للتطوير بدون تغيير هيكل التنقل."
        }
        let stack = UIStackView(arrangedSubviews: [title, note]); stack.axis = .vertical; stack.spacing = 14; view.addSubview(stack); stack.translatesAutoresizingMaskIntoConstraints = false
        NSLayoutConstraint.activate([stack.centerYAnchor.constraint(equalTo: view.safeAreaLayoutGuide.centerYAnchor), stack.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 32), stack.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -32)])
    }

    private var displayName: String {
        switch destination {
        case .training: return "التدريب"
        case .operations: return "العمليات"
        case .loadout: return "العتاد"
        case .settings: return "الإعدادات"
        case .diagnostics: return "مركز الأرصاد"
        case .game: return "اللعبة"
        case .updates: return "مركز التحديث"
        }
    }
}
