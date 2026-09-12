import UIKit

final class METSECrashReportViewController: UIViewController {
    private let crashText: String
    init(crashText: String) { self.crashText = crashText; super.init(nibName: nil, bundle: nil); modalPresentationStyle = .overFullScreen }
    @available(*, unavailable) required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
    override var prefersStatusBarHidden: Bool { true }
    override var supportedInterfaceOrientations: UIInterfaceOrientationMask { .landscape }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = UIColor(red: 0.02, green: 0.006, blue: 0.006, alpha: 0.98)

        let title = UILabel()
        title.text = "⚠️ التطبيق انهار آخر مرة"
        title.font = .systemFont(ofSize: 22, weight: .bold)
        title.textColor = .white

        let subtitle = UILabel()
        subtitle.text = "انسخ التقرير وابعثه لتشخيص السبب."
        subtitle.font = .systemFont(ofSize: 13, weight: .medium)
        subtitle.textColor = UIColor.white.withAlphaComponent(0.6)

        let textView = UITextView()
        textView.text = crashText
        textView.font = .monospacedSystemFont(ofSize: 11, weight: .regular)
        textView.textColor = UIColor(red: 1, green: 0.62, blue: 0.55, alpha: 1)
        textView.backgroundColor = UIColor.white.withAlphaComponent(0.05)
        textView.layer.cornerRadius = 12
        textView.isEditable = false
        textView.textContainerInset = UIEdgeInsets(top: 12, left: 12, bottom: 12, right: 12)

        let copyButton = UIButton(type: .system)
        var copyConfig = UIButton.Configuration.filled()
        copyConfig.title = "نسخ التقرير"
        copyConfig.image = UIImage(systemName: "doc.on.doc")
        copyConfig.imagePadding = 8
        copyConfig.baseBackgroundColor = UIColor(red: 0.18, green: 0.56, blue: 0.42, alpha: 1)
        copyButton.configuration = copyConfig
        copyButton.addAction(UIAction { [weak self, weak copyButton] _ in
            guard let self else { return }
            UIPasteboard.general.string = self.crashText
            var updated = copyButton?.configuration
            updated?.title = "تم النسخ"
            copyButton?.configuration = updated
        }, for: .touchUpInside)

        let shareButton = UIButton(type: .system)
        var shareConfig = UIButton.Configuration.plain()
        shareConfig.title = "مشاركة"
        shareConfig.image = UIImage(systemName: "square.and.arrow.up")
        shareConfig.imagePadding = 8
        shareButton.configuration = shareConfig
        shareButton.tintColor = .white
        shareButton.addAction(UIAction { [weak self, weak shareButton] _ in
            guard let self else { return }
            let activity = UIActivityViewController(activityItems: [self.crashText], applicationActivities: nil)
            activity.popoverPresentationController?.sourceView = shareButton
            self.present(activity, animated: true)
        }, for: .touchUpInside)

        let closeButton = UIButton(type: .system)
        var closeConfig = UIButton.Configuration.plain()
        closeConfig.title = "إغلاق"
        closeButton.configuration = closeConfig
        closeButton.tintColor = UIColor.white.withAlphaComponent(0.6)
        closeButton.addAction(UIAction { [weak self] _ in self?.dismiss(animated: true) }, for: .touchUpInside)

        let buttonRow = UIStackView(arrangedSubviews: [copyButton, shareButton, closeButton])
        buttonRow.axis = .horizontal
        buttonRow.spacing = 12

        let stack = UIStackView(arrangedSubviews: [title, subtitle, textView, buttonRow])
        stack.axis = .vertical
        stack.spacing = 14
        stack.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(stack)
        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 28),
            stack.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -28),
            stack.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 20),
            stack.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -20)
        ])
    }
}
