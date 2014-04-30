using System.Web;
using System.Web.Optimization;

namespace WebApiApp
{
    public class BundleConfig
    {
        // For more information on Bundling, visit http://go.microsoft.com/fwlink/?LinkId=254725
        public static void RegisterBundles(BundleCollection bundles)
        {
            bundles.Add(new ScriptBundle("~/bundles/jquery").Include(
                        "~/Scripts/jquery-1.9.1.js"));
            bundles.Add(new ScriptBundle("~/bundles/bootstrapjs").Include(
                        "~/Scripts/bootstrap.js"));
            bundles.Add(new ScriptBundle("~/bundles/jqueryval").Include(
                        "~/Scripts/jquery.unobtrusive*",
                        "~/Scripts/jquery.validate*"));
            bundles.Add(new ScriptBundle("~/bundles/d3js").Include(
                        "~/Scripts/d3.v3.js"));
            bundles.Add(new ScriptBundle("~/bundles/SignalRjs").Include(
                        "~/Scripts/jquery.signalR-2.0.1.js"));

            bundles.Add(new StyleBundle("~/Content/bootstrapcss").Include(
                        "~/Content/bootstrap.css"));
            bundles.Add(new StyleBundle("~/Content/chartcss").Include(
                        "~/Content/chart.css"));
        }
    }
}