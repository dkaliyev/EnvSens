using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Web.Http;
using WebApiApp.Models;

namespace WebApiApp.Controllers
{
    public class TimeController : ApiController
    {
        public long Get()
        {
            TimeSpan TS = (TimeSpan)(DateTime.Now - new DateTime(1970, 1, 1));
            long ms = (long)TS.TotalMilliseconds;
            return ms;
        }
    }
}
