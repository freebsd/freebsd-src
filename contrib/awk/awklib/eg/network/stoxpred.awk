function ReadQuotes() {
  # Retrieve historical data for each ticker symbol
  FS = ","
  for (stock = 1; stock <= StockCount; stock++) {
    URL = "http://chart.yahoo.com/table.csv?s=" name[stock] \
          "&a=" month "&b=" day   "&c=" year-1 \
          "&d=" month "&e=" day   "&f=" year \
          "g=d&q=q&y=0&z=" name[stock] "&x=.csv"
    printf("GET " URL " HTTP/1.0\r\n\r\n") |& YahooData
    while ((YahooData |& getline) > 0) {
      if (NF == 6 && $1 ~ /Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec/) {
        if (stock == 1)
          days[++daycount] = $1;
        quote[$1, stock] = $5
      }
    }
    close(YahooData)
  }
  FS = " "
}
function CleanUp() {
  # clean up time series; eliminate incomplete data sets
  for (d = 1; d <= daycount; d++) {
    for (stock = 1; stock <= StockCount; stock++)
      if (! ((days[d], stock) in quote))
          stock = StockCount + 10
    if (stock > StockCount + 1)
        continue
    datacount++
    for (stock = 1; stock <= StockCount; stock++)
      data[datacount, stock] = int(0.5 + quote[days[d], stock])
  }
  delete quote
  delete days
}
function Prediction() {
  # Predict each ticker symbol by prolonging yesterday's trend
  for (stock = 1; stock <= StockCount; stock++) {
    if         (data[1, stock] > data[2, stock]) {
      predict[stock] = "up"
    } else if  (data[1, stock] < data[2, stock]) { 
      predict[stock] = "down" 
    } else {
      predict[stock] = "neutral"
    }
    if ((data[1, stock] > data[2, stock]) && (data[2, stock] > data[3, stock]))
      hot[stock] = 1
    if ((data[1, stock] < data[2, stock]) && (data[2, stock] < data[3, stock]))
      avoid[stock] = 1  
  }
  # Do a plausibility check: how many predictions proved correct?
  for (s = 1; s <= StockCount; s++) {
    for (d = 1; d <= datacount-2; d++) {
      if         (data[d+1, s] > data[d+2, s]) {
        UpCount++
      } else if  (data[d+1, s] < data[d+2, s]) {
        DownCount++
      } else {
        NeutralCount++
      }   
      if (((data[d, s]  > data[d+1, s]) && (data[d+1, s]  > data[d+2, s])) ||
          ((data[d, s]  < data[d+1, s]) && (data[d+1, s]  < data[d+2, s])) ||
          ((data[d, s] == data[d+1, s]) && (data[d+1, s] == data[d+2, s])))
        CorrectCount++
    }   
  }       
}
function Report() {
  # Generate report
  report =        "\nThis is your daily "
  report = report "stock market report for "strftime("%A, %B %d, %Y")".\n"
  report = report "Here are the predictions for today:\n\n"
  for (stock = 1; stock <= StockCount; stock++)  
    report = report "\t" name[stock] "\t" predict[stock] "\n"
  for (stock in hot) {
    if (HotCount++ == 0)
      report = report "\nThe most promising shares for today are these:\n\n"
    report = report "\t" name[stock] "\t\thttp://biz.yahoo.com/n/" \
      tolower(substr(name[stock], 1, 1)) "/" tolower(name[stock]) ".html\n"
  }
  for (stock in avoid) {
    if (AvoidCount++ == 0)
      report = report "\nThe stock shares to avoid today are these:\n\n"
    report = report "\t" name[stock] "\t\thttp://biz.yahoo.com/n/" \
      tolower(substr(name[stock], 1, 1)) "/" tolower(name[stock]) ".html\n"
  }   
  report = report "\nThis sums up to " HotCount+0 " winners and " AvoidCount+0
  report = report " losers. When using this kind\nof prediction scheme for"
  report = report " the 12 months which lie behind us,\nwe get " UpCount
  report = report " 'ups' and " DownCount " 'downs' and " NeutralCount
  report = report " 'neutrals'. Of all\nthese " UpCount+DownCount+NeutralCount
  report = report " predictions " CorrectCount " proved correct next day.\n"
  report = report "A success rate of "\
             int(100*CorrectCount/(UpCount+DownCount+NeutralCount)) "%.\n"
  report = report "Random choice would have produced a 33% success rate.\n"
  report = report "Disclaimer: Like every other prediction of the stock\n"
  report = report "market, this report is, of course, complete nonsense.\n"
  report = report "If you are stupid enough to believe these predictions\n"
  report = report "you should visit a doctor who can treat your ailment."
}     
function SendMail() { 
  # send report to customers
  customer["uncle.scrooge@ducktown.gov"] = "Uncle Scrooge"
  customer["more@utopia.org"           ] = "Sir Thomas More"
  customer["spinoza@denhaag.nl"        ] = "Baruch de Spinoza"
  customer["marx@highgate.uk"          ] = "Karl Marx"
  customer["keynes@the.long.run"       ] = "John Maynard Keynes"
  customer["bierce@devil.hell.org"     ] = "Ambrose Bierce"
  customer["laplace@paris.fr"          ] = "Pierre Simon de Laplace"
  for (c in customer) {
    MailPipe = "mail -s 'Daily Stock Prediction Newsletter'" c
    print "Good morning " customer[c] "," | MailPipe
    print report "\n.\n" | MailPipe
    close(MailPipe)
  }
}   
