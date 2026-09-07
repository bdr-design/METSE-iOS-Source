import UIKit

final class GatewayViewController: UIViewController {
    private let gradient = CAGradientLayer()
    override func viewDidLoad() { super.viewDidLoad(); configureBackground(); configureLayout() }
    override func viewWillAppear(_ animated: Bool) { super.viewWillAppear(animated); navigationController?.setNavigationBarHidden(true, animated: animated) }
    override func viewDidLayoutSubviews() { super.viewDidLayoutSubviews(); gradient.frame=view.bounds }

    private func configureBackground(){ view.backgroundColor=UIColor(red:0.006,green:0.012,blue:0.014,alpha:1); gradient.colors=[UIColor(red:0.018,green:0.065,blue:0.055,alpha:1).cgColor,UIColor(red:0.006,green:0.012,blue:0.014,alpha:1).cgColor]; gradient.startPoint=CGPoint(x:0,y:0);gradient.endPoint=CGPoint(x:1,y:1);view.layer.insertSublayer(gradient,at:0) }
    private func configureLayout(){
        let root=UIStackView(); root.axis=.horizontal; root.spacing=24; root.distribution=.fillEqually; view.addSubview(root); root.translatesAutoresizingMaskIntoConstraints=false
        NSLayoutConstraint.activate([root.leadingAnchor.constraint(equalTo:view.safeAreaLayoutGuide.leadingAnchor,constant:28),root.trailingAnchor.constraint(equalTo:view.safeAreaLayoutGuide.trailingAnchor,constant:-28),root.topAnchor.constraint(equalTo:view.safeAreaLayoutGuide.topAnchor,constant:22),root.bottomAnchor.constraint(equalTo:view.safeAreaLayoutGuide.bottomAnchor,constant:-22)])
        root.addArrangedSubview(makeHero()); root.addArrangedSubview(makeModules())
    }
    private func makeHero()->UIView{
        let eyebrow=UILabel(); eyebrow.text="MIDDLE EAST TACTICAL SIMULATION ENGINE"; eyebrow.font=.monospacedSystemFont(ofSize:11,weight:.semibold); eyebrow.textColor=UIColor(red:0.46,green:0.82,blue:0.67,alpha:1)
        let title=UILabel(); title.text="METSE"; title.font=.systemFont(ofSize:54,weight:.black); title.textColor=.white
        let sub=UILabel(); sub.text="مركز القيادة"; sub.font=.systemFont(ofSize:25,weight:.semibold); sub.textColor=UIColor.white.withAlphaComponent(0.70)
        var cfg=UIButton.Configuration.filled(); cfg.title="ابدأ جلسة تكتيكية"; cfg.subtitle="دخول مباشر إلى المحرك"; cfg.image=UIImage(systemName:"play.fill"); cfg.imagePadding=14; cfg.cornerStyle=.large; cfg.baseBackgroundColor=UIColor(red:0.18,green:0.56,blue:0.42,alpha:1); cfg.contentInsets=NSDirectionalEdgeInsets(top:18,leading:22,bottom:18,trailing:22)
        let play=UIButton(configuration:cfg); play.contentHorizontalAlignment=.leading; play.heightAnchor.constraint(greaterThanOrEqualToConstant:82).isActive=true; play.addAction(UIAction{[weak self]_ in self?.openGame()},for:.touchUpInside)
        let meta=UILabel(); meta.text="BUILD 003  •  NATIVE METAL  •  CONTENT DEV  •  32 MAX"; meta.font=.monospacedSystemFont(ofSize:10,weight:.medium); meta.textColor=UIColor.white.withAlphaComponent(0.42); meta.numberOfLines=2
        let stack=UIStackView(arrangedSubviews:[eyebrow,title,sub,UIView(),play,meta]); stack.axis=.vertical; stack.spacing=8; return stack
    }
    private func makeModules()->UIView{
        let heading=UILabel(); heading.text="الأنظمة"; heading.font=.systemFont(ofSize:20,weight:.bold); heading.textColor=.white
        let grid=UIStackView(); grid.axis=.vertical; grid.spacing=10
        let modules=GatewayModule.secondary
        for rowStart in stride(from:0,to:modules.count,by:2){ let row=UIStackView(); row.axis=.horizontal; row.spacing=10; row.distribution=.fillEqually; for i in rowStart..<min(rowStart+2,modules.count){ let m=modules[i]; let card=GatewayCardButton(module:m); card.addAction(UIAction{[weak self]_ in self?.open(m.destination)},for:.touchUpInside); row.addArrangedSubview(card) }; if row.arrangedSubviews.count==1{row.addArrangedSubview(UIView())}; grid.addArrangedSubview(row) }
        let s=UIStackView(arrangedSubviews:[heading,grid]); s.axis=.vertical; s.spacing=12; return s
    }
    private func openGame(){ navigationController?.pushViewController(GameViewController(),animated:true) }
    private func open(_ d:GatewayModule.Destination){ navigationController?.setNavigationBarHidden(false,animated:true); switch d{case .game:openGame();case .updates:navigationController?.pushViewController(UpdateCenterViewController(),animated:true);default:navigationController?.pushViewController(ModulePlaceholderViewController(destination:d),animated:true)} }
}
